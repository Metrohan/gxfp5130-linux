/*
 * Unit tests for SIGFM template update (fpi_print_sigfm_adapt)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <libfprint/fprint.h>

#include "fpi-print.h"
#include "fp-print-private.h"
#include "sigfm/sigfm.hpp"

#define THRESHOLD 40
#define W 80
#define H 64

/* A blurred noise frame; the same seed gives the same "finger". */
static SigfmImgInfo *
frame (guint32 seed)
{
  g_autoptr(GRand) rand = g_rand_new_with_seed (seed);
  guchar pix[W * H], tmp[W * H];

  for (guint i = 0; i < W * H; i++)
    pix[i] = g_rand_int_range (rand, 0, 256);

  for (guint pass = 0; pass < 2; pass++)
    {
      for (gint y = 0; y < H; y++)
        for (gint x = 0; x < W; x++)
          {
            guint sum = 0, n = 0;
            for (gint dy = -1; dy <= 1; dy++)
              for (gint dx = -1; dx <= 1; dx++)
                if (x + dx >= 0 && x + dx < W && y + dy >= 0 && y + dy < H)
                  sum += pix[(y + dy) * W + x + dx], n++;
            tmp[y * W + x] = sum / n;
          }
      memcpy (pix, tmp, sizeof (pix));
    }

  return sigfm_extract (pix, W, H);
}

/* A SIGFM print from the frames for @seeds; the last @adapted were added. */
static FpPrint *
make_print (const guint32 *seeds, guint n, guint adapted)
{
  FpPrint *print = g_object_new (FP_TYPE_PRINT,
                                 "driver", "test",
                                 "device-id", "0",
                                 NULL);

  g_object_ref_sink (print);
  fpi_print_set_type (print, FPI_PRINT_SIGFM);
  for (guint i = 0; i < n; i++)
    g_ptr_array_add (print->prints, frame (seeds[i]));
  print->sigfm_adapted = adapted;
  return print;
}

static FpPrint *
probe (guint32 seed)
{
  return make_print (&seed, 1, 0);
}

/* Whether sample @i of @print came from the frame for @seed. */
static gboolean
sample_is (FpPrint *print, guint i, guint32 seed)
{
  SigfmImgInfo *want = frame (seed);
  int la, lb;
  g_autofree guchar *a = sigfm_serialize_binary (g_ptr_array_index (print->prints, i), &la);
  g_autofree guchar *b = sigfm_serialize_binary (want, &lb);

  sigfm_free_info (want);
  return la == lb && memcmp (a, b, la) == 0;
}

static void
test_frames_sane (void)
{
  SigfmImgInfo *a = frame (1), *a2 = frame (1), *b = frame (2);

  g_assert_cmpint (sigfm_match_score (a, a2), >=, THRESHOLD);
  g_assert_cmpint (sigfm_match_score (a, b), <, THRESHOLD);
  sigfm_free_info (a);
  sigfm_free_info (a2);
  sigfm_free_info (b);
}

static void
test_adds_and_leaves_template (void)
{
  const guint32 seeds[] = { 1, 1, 1, 2 };
  g_autoptr(FpPrint) template = make_print (seeds, 4, 0);
  g_autoptr(FpPrint) scan = probe (1);
  g_autoptr(FpPrint) updated = fpi_print_sigfm_adapt (template, scan, THRESHOLD, 3, 8);

  g_assert_nonnull (updated);
  g_assert_cmpuint (updated->prints->len, ==, 5);
  g_assert_cmpuint (updated->sigfm_adapted, ==, 1);
  g_assert_true (sample_is (updated, 4, 1));

  g_assert_cmpuint (template->prints->len, ==, 4);
  g_assert_cmpuint (template->sigfm_adapted, ==, 0);
}

static void
test_needs_enrolled_support (void)
{
  const guint32 seeds[] = { 1, 1, 2, 2 };
  g_autoptr(FpPrint) template = make_print (seeds, 4, 0);
  g_autoptr(FpPrint) scan = probe (1);

  g_assert_null (fpi_print_sigfm_adapt (template, scan, THRESHOLD, 3, 8));
}

static void
test_added_samples_give_no_support (void)
{
  /* Enrolled: finger 2. Added (say by mistake): finger 1, three times. */
  const guint32 seeds[] = { 2, 2, 2, 1, 1, 1 };
  g_autoptr(FpPrint) template = make_print (seeds, 6, 3);
  g_autoptr(FpPrint) scan = probe (1);

  g_assert_null (fpi_print_sigfm_adapt (template, scan, THRESHOLD, 1, 8));
}

static void
test_evicts_oldest_added (void)
{
  const guint32 seeds[] = { 1, 2, 3 };
  g_autoptr(FpPrint) template = make_print (seeds, 3, 0);

  for (guint32 seed = 1; seed <= 3; seed++)
    {
      g_autoptr(FpPrint) scan = probe (seed);
      FpPrint *updated = fpi_print_sigfm_adapt (template, scan, THRESHOLD, 1, 2);

      g_assert_nonnull (updated);
      g_object_unref (template);
      template = updated;
    }

  /* Enrolled 1, 2, 3 kept; added 1 evicted, 2 and 3 kept. */
  g_assert_cmpuint (template->prints->len, ==, 5);
  g_assert_cmpuint (template->sigfm_adapted, ==, 2);
  for (guint i = 0; i < 3; i++)
    g_assert_true (sample_is (template, i, i + 1));
  g_assert_true (sample_is (template, 3, 2));
  g_assert_true (sample_is (template, 4, 3));
}

static void
test_cap_holds_for_overfull_print (void)
{
  /* 4 enrolled, 4 added, but only 2 added samples allowed. */
  const guint32 seeds[] = { 1, 1, 1, 1, 5, 6, 7, 8 };
  g_autoptr(FpPrint) template = make_print (seeds, 8, 4);
  g_autoptr(FpPrint) scan = probe (1);
  g_autoptr(FpPrint) updated = fpi_print_sigfm_adapt (template, scan, THRESHOLD, 3, 2);

  g_assert_nonnull (updated);
  g_assert_cmpuint (updated->prints->len, ==, 6);
  g_assert_cmpuint (updated->sigfm_adapted, ==, 2);
  g_assert_true (sample_is (updated, 4, 8));
  g_assert_true (sample_is (updated, 5, 1));
}

static void
test_serialize_keeps_added_count (void)
{
  const guint32 seeds[] = { 1, 2, 3 };
  g_autoptr(FpPrint) old = make_print (seeds, 3, 0);
  g_autoptr(FpPrint) adapted = make_print (seeds, 3, 1);
  g_autoptr(FpPrint) old2 = NULL;
  g_autoptr(FpPrint) adapted2 = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree guchar *data = NULL;
  gsize len;

  g_assert_true (fp_print_serialize (old, &data, &len, &error));
  old2 = fp_print_deserialize (data, len, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (old2->prints->len, ==, 3);
  g_assert_cmpuint (old2->sigfm_adapted, ==, 0);

  g_clear_pointer (&data, g_free);
  g_assert_true (fp_print_serialize (adapted, &data, &len, &error));
  adapted2 = fp_print_deserialize (data, len, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (adapted2->sigfm_adapted, ==, 1);
}

static void
test_enroll_inserts_before_added (void)
{
  const guint32 seeds[] = { 1, 2, 3 };
  g_autoptr(FpPrint) template = make_print (seeds, 3, 1);
  g_autoptr(FpPrint) scan = probe (4);

  fpi_print_add_print (template, scan);
  g_assert_cmpuint (template->prints->len, ==, 4);
  g_assert_cmpuint (template->sigfm_adapted, ==, 1);
  g_assert_true (sample_is (template, 2, 4));
  g_assert_true (sample_is (template, 3, 3));
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/print/sigfm-adapt/frames-sane", test_frames_sane);
  g_test_add_func ("/print/sigfm-adapt/adds-and-leaves-template", test_adds_and_leaves_template);
  g_test_add_func ("/print/sigfm-adapt/needs-enrolled-support", test_needs_enrolled_support);
  g_test_add_func ("/print/sigfm-adapt/added-samples-give-no-support", test_added_samples_give_no_support);
  g_test_add_func ("/print/sigfm-adapt/evicts-oldest-added", test_evicts_oldest_added);
  g_test_add_func ("/print/sigfm-adapt/cap-holds-for-overfull-print", test_cap_holds_for_overfull_print);
  g_test_add_func ("/print/sigfm-adapt/serialize-keeps-added-count", test_serialize_keeps_added_count);
  g_test_add_func ("/print/sigfm-adapt/enroll-inserts-before-added", test_enroll_inserts_before_added);

  return g_test_run ();
}
