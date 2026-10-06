/*
 * Unit tests for FpPrint serialization
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

/* Saving a SIGFM print used to call g_clear_object() on its GPtrArray of
 * temporary buffers: a critical (fatal under G_DEBUG=fatal-warnings) and a
 * leak on every save. */
static void
test_sigfm_serialize_roundtrip (void)
{
  g_autoptr(GRand) rand = g_rand_new_with_seed (1);
  g_autoptr(FpPrint) print = NULL;
  g_autoptr(FpPrint) loaded = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree guchar *data = NULL;
  guchar pix[80 * 64];
  gsize len;

  for (guint i = 0; i < G_N_ELEMENTS (pix); i++)
    pix[i] = g_rand_int_range (rand, 0, 256);

  print = g_object_ref_sink (g_object_new (FP_TYPE_PRINT,
                                           "driver", "test",
                                           "device-id", "0",
                                           NULL));
  fpi_print_set_type (print, FPI_PRINT_SIGFM);
  g_ptr_array_add (print->prints, sigfm_extract (pix, 80, 64));

  g_assert_true (fp_print_serialize (print, &data, &len, &error));
  g_assert_no_error (error);

  loaded = fp_print_deserialize (data, len, &error);
  g_assert_no_error (error);
  g_assert_cmpint (loaded->type, ==, FPI_PRINT_SIGFM);
  g_assert_cmpuint (loaded->prints->len, ==, 1);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/print/sigfm-serialize-roundtrip", test_sigfm_serialize_roundtrip);

  return g_test_run ();
}
