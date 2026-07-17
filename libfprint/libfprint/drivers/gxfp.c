#define FP_COMPONENT "gxfp"

#include "drivers_api.h"

#include "../fpi-image-device.h"

#include <errno.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

#include "gxfp/algo/common.h"
#include "gxfp/flow/session.h"

#define DEFAULT_ENROLL_SAMPLES 16
#define GXFP_DEFAULT_PSK_PATH  "/var/lib/fprintd/gxfp/psk_raw32.bin"

struct _FpiDeviceGxfp {
  FpImageDevice parent;

  struct gxfp_session sess;

  GSource *timer_source;

  gchar *dev_path;
  gchar *psk_path;
};

G_DECLARE_FINAL_TYPE (FpiDeviceGxfp, fpi_device_gxfp, FPI, DEVICE_GXFP, FpImageDevice)
G_DEFINE_TYPE (FpiDeviceGxfp, fpi_device_gxfp, FP_TYPE_IMAGE_DEVICE)

static void schedule_pump_soon (FpiDeviceGxfp *self, gint delay_ms);
static void fpi_device_gxfp_dispose (GObject *object);

static void
cancel_scheduled_pump (FpiDeviceGxfp *self)
{
  if (!self || !self->timer_source)
    return;

  g_source_destroy (self->timer_source);
  self->timer_source = NULL;
}

static void
free_paths (FpiDeviceGxfp *self)
{
  if (!self)
    return;

  g_clear_pointer (&self->dev_path, g_free);
  g_clear_pointer (&self->psk_path, g_free);
}

static void
emit_image_captured (FpiDeviceGxfp *self)
{
  struct gxfp_decoded_image img;
  FpImage *fp_img;
  int r;

  memset (&img, 0, sizeof (img));
  r = gxfp_session_take_image (&self->sess, &img);
  if (r < 0)
    {
      fpi_image_device_session_error (FP_IMAGE_DEVICE (self),
                                      fpi_device_error_new_msg (FP_DEVICE_ERROR_GENERAL,
                                                                "capture image unavailable: %s",
                                                                g_strerror (-r)));
      return;
    }

  fp_img = fp_image_new (img.cols, img.rows);
  for (gint y = 0; y < img.rows; y++)
    for (gint x = 0; x < img.cols; x++)
      {
        guint16 p = img.pixels[(gsize) y * img.cols + x];
        fp_img->data[(gsize) y * img.cols + x] = (guint8) ((p * 255u) / 4095u);
      }

  gxfp_decoded_image_free (&img);
  fpi_image_device_image_captured (FP_IMAGE_DEVICE (self), fp_img);
}

static void
handle_session_events (FpiDeviceGxfp *self, struct gxfp_session_events *ev)
{
  if (!self || !ev)
    return;

  if (ev->cancel_tick)
    cancel_scheduled_pump (self);

  if (ev->finger_status_changed)
    fpi_image_device_report_finger_status (FP_IMAGE_DEVICE (self), ev->finger_present ? TRUE : FALSE);

  if (ev->image_ready)
    emit_image_captured (self);

  if (ev->session_error)
    {
      GError *err = fpi_device_error_new_msg (FP_DEVICE_ERROR_GENERAL,
                                              "%s",
                                              ev->error_msg[0] ? ev->error_msg : "gxfp session error");
      cancel_scheduled_pump (self);

      if (ev->error_target == GXFP_SESSION_ERR_TARGET_ACTIVATE)
        fpi_image_device_activate_complete (FP_IMAGE_DEVICE (self), err);
      else
        fpi_image_device_session_error (FP_IMAGE_DEVICE (self), err);

      return;
    }

  if (ev->activate_complete)
    fpi_image_device_activate_complete (FP_IMAGE_DEVICE (self), NULL);

  if (ev->deactivate_complete)
    {
      cancel_scheduled_pump (self);
      fpi_image_device_deactivate_complete (FP_IMAGE_DEVICE (self), NULL);
    }

  if (ev->close_complete)
    {
      cancel_scheduled_pump (self);
      free_paths (self);
      fpi_image_device_close_complete (FP_IMAGE_DEVICE (self), NULL);
    }

  if (ev->request_tick)
    schedule_pump_soon (self, ev->tick_delay_ms);
}

static void
gxfp_pump_timeout_cb (FpDevice *device, gpointer user_data)
{
  FpiDeviceGxfp *self = FPI_DEVICE_GXFP (user_data);
  struct gxfp_session_events ev;
  int cancelled;
  int pr;

  (void) device;
  if (!self)
    return;

  self->timer_source = NULL;
  gxfp_session_events_clear (&ev);
  cancelled = fpi_device_action_is_cancelled (FP_DEVICE (self)) ? 1 : 0;
  gxfp_session_pump (&self->sess, cancelled, &ev);
  handle_session_events (self, &ev);

  if (fpi_device_action_is_cancelled (FP_DEVICE (self)))
    return;

  pr = gxfp_session_poll_readable (&self->sess, 0);
  if (pr == 0 || pr == -EAGAIN)
    {
      gxfp_session_events_clear (&ev);
      gxfp_session_on_fd (&self->sess, GXFP_SESSION_IO_IN, cancelled, &ev);
      handle_session_events (self, &ev);
    }
}

static void
schedule_pump_soon (FpiDeviceGxfp *self, gint delay_ms)
{
  if (!self || self->timer_source)
    return;

  self->timer_source = fpi_device_add_timeout (FP_DEVICE (self),
                                               delay_ms,
                                               gxfp_pump_timeout_cb,
                                               self,
                                               NULL);
}

static void
gxfp_img_open (FpImageDevice *dev)
{
  FpiDeviceGxfp *self = FPI_DEVICE_GXFP (dev);
  g_autoptr(GError) error = NULL;
  uint8_t *psk_raw = NULL;
  size_t psk_raw_len = 0;
  char errbuf[256] = { 0 };
  const gchar *chardev_path;
  const gchar *psk_env;
  int r;

  g_return_if_fail (self);

  chardev_path = fpi_device_get_udev_data (FP_DEVICE (self), FPI_DEVICE_UDEV_SUBTYPE_CHARDEV);
  if (!chardev_path || chardev_path[0] == '\0')
    {
      g_set_error (&error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "no chardev path from udev enumeration");
      fpi_image_device_open_complete (dev, g_steal_pointer (&error));
      return;
    }

  free_paths (self);
  self->dev_path = g_strdup (chardev_path);

  psk_env = g_getenv ("FP_GXFP_PSK");
  if (!psk_env || psk_env[0] == '\0')
    psk_env = GXFP_DEFAULT_PSK_PATH;
  self->psk_path = g_strdup (psk_env);

  r = gxfp_read_file_all (self->psk_path, &psk_raw, &psk_raw_len);
  if (r < 0)
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   g_io_error_from_errno (-r),
                   "read PSK '%s' failed: %s",
                   self->psk_path,
                   g_strerror (-r));
      fpi_image_device_open_complete (dev, g_steal_pointer (&error));
      return;
    }

  r = gxfp_session_open (&self->sess,
                         self->dev_path,
                         psk_raw,
                         psk_raw_len,
                         g_getenv ("FP_GXFP_LOG") ? 1 : 0,
                         errbuf,
                         sizeof (errbuf));
  free (psk_raw);
  if (r < 0)
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   g_io_error_from_errno (-r),
                   "%s",
                   errbuf[0] ? errbuf : "gxfp_session_open failed");
      fpi_image_device_open_complete (dev, g_steal_pointer (&error));
      return;
    }

  fpi_image_device_open_complete (dev, NULL);
}

static void
gxfp_img_close (FpImageDevice *dev)
{
  FpiDeviceGxfp *self = FPI_DEVICE_GXFP (dev);
  struct gxfp_session_events ev;

  if (!self)
    return;

  cancel_scheduled_pump (self);

  gxfp_session_events_clear (&ev);
  gxfp_session_request_close (&self->sess, 0, &ev);
  handle_session_events (self, &ev);
}

static void
gxfp_activate (FpImageDevice *dev)
{
  FpiDeviceGxfp *self = FPI_DEVICE_GXFP (dev);
  struct gxfp_session_events ev;
  int r;

  if (!self)
    return;

  gxfp_session_events_clear (&ev);
  r = gxfp_session_activate (&self->sess, g_getenv ("FP_GXFP_LOG") ? 1 : 0, &ev);
  handle_session_events (self, &ev);
  if (r < 0)
    return;

  schedule_pump_soon (self, 0);
}

static void
gxfp_deactivate (FpImageDevice *dev)
{
  FpiDeviceGxfp *self = FPI_DEVICE_GXFP (dev);
  struct gxfp_session_events ev;

  if (!self)
    return;

  cancel_scheduled_pump (self);

  gxfp_session_events_clear (&ev);
  gxfp_session_request_deactivate (&self->sess, &ev);
  handle_session_events (self, &ev);
}

static void
gxfp_change_state (FpImageDevice *dev, FpiImageDeviceState state)
{
  FpiDeviceGxfp *self = FPI_DEVICE_GXFP (dev);
  struct gxfp_session_events ev;
  enum gxfp_session_state new_state = GXFP_SESSION_STATE_IDLE;

  if (!self)
    return;

  switch (state)
    {
    case FPI_IMAGE_DEVICE_STATE_INACTIVE:
      new_state = GXFP_SESSION_STATE_INACTIVE;
      break;

    case FPI_IMAGE_DEVICE_STATE_ACTIVATING:
    case FPI_IMAGE_DEVICE_STATE_DEACTIVATING:
    case FPI_IMAGE_DEVICE_STATE_IDLE:
      new_state = GXFP_SESSION_STATE_IDLE;
      break;

    case FPI_IMAGE_DEVICE_STATE_AWAIT_FINGER_ON:
      new_state = GXFP_SESSION_STATE_AWAIT_FINGER_ON;
      break;

    case FPI_IMAGE_DEVICE_STATE_CAPTURE:
      new_state = GXFP_SESSION_STATE_CAPTURE;
      break;

    case FPI_IMAGE_DEVICE_STATE_AWAIT_FINGER_OFF:
      new_state = GXFP_SESSION_STATE_AWAIT_FINGER_OFF;
      break;
    }

  gxfp_session_events_clear (&ev);
  gxfp_session_change_state (&self->sess, new_state, &ev);
  handle_session_events (self, &ev);
}

static const FpIdEntry driver_ids[] = {
  { .udev_types = FPI_DEVICE_UDEV_SUBTYPE_CHARDEV, .chardev_acpi_id = "GXFP5130" },
  { .udev_types = 0 },
};

static void
fpi_device_gxfp_class_init (FpiDeviceGxfpClass *klass)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);
  FpDeviceClass *dev_class = FP_DEVICE_CLASS (klass);
  FpImageDeviceClass *img_class = FP_IMAGE_DEVICE_CLASS (klass);

  obj_class->dispose = fpi_device_gxfp_dispose;

  dev_class->id = FP_COMPONENT;
  dev_class->full_name = "Goodix GXFP5130 eSPI Fingerprint Sensor";
  dev_class->type = FP_DEVICE_TYPE_UDEV;
  dev_class->id_table = driver_ids;
  dev_class->scan_type = FP_SCAN_TYPE_PRESS;
  dev_class->nr_enroll_stages = DEFAULT_ENROLL_SAMPLES;

  img_class->img_width = 176;
  img_class->img_height = 54;
  img_class->algorithm = FPI_DEVICE_ALGO_SIGFM;

  img_class->img_open = gxfp_img_open;
  img_class->img_close = gxfp_img_close;
  img_class->activate = gxfp_activate;
  img_class->deactivate = gxfp_deactivate;
  img_class->change_state = gxfp_change_state;
}

static void
fpi_device_gxfp_init (FpiDeviceGxfp *self)
{
  if (!self)
    return;

  memset (&self->sess, 0, sizeof (self->sess));
  gxfp_session_init (&self->sess);
  self->timer_source = NULL;
}

static void
fpi_device_gxfp_dispose (GObject *object)
{
  FpiDeviceGxfp *self = FPI_DEVICE_GXFP (object);

  if (self)
    {
      cancel_scheduled_pump (self);
      gxfp_session_dispose (&self->sess);
      free_paths (self);
    }

  G_OBJECT_CLASS (fpi_device_gxfp_parent_class)->dispose (object);
}
