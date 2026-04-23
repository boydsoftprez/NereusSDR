/* rnnoise_model_init.c — NereusSDR
 *
 * Provides init_rnnoise(), which wires model layer names from a
 * WeightArray (loaded at runtime from a .bin file) into the RNNoise
 * model struct.  This function normally lives in rnnoise_data_little.c /
 * rnnoise_data.c, but those files also contain the baked-in weight data
 * (274 000+ lines of float arrays). We exclude the baked data because
 * NereusSDR loads the model at runtime via rnnoise_model_from_filename(),
 * so the baked arrays are dead code.
 *
 * Ported from:
 *   Thetis Project Files/lib/NR_Algorithms_x64/src/rnnoise/src/rnnoise_data_little.c
 *   lines 274936-274952
 *   Thetis v2.10.3.13 @ 501e3f51 — verified 2026-04-23.
 *
 * Original licence: BSD 3-clause (same as the rest of rnnoise).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "rnnoise_data.h"
#include "nnet.h"

#ifndef DUMP_BINARY_WEIGHTS
int init_rnnoise(RNNoise *model, const WeightArray *arrays) {
    if (linear_init(&model->conv1, arrays, "conv1_bias", NULL, NULL,"conv1_weights_float", NULL, NULL, NULL, 195, 128)) return 1;
    if (linear_init(&model->conv2, arrays, "conv2_bias", "conv2_subias", "conv2_weights_int8","conv2_weights_float", NULL, NULL, "conv2_scale", 384, 384)) return 1;
    if (linear_init(&model->gru1_input, arrays, "gru1_input_bias", "gru1_input_subias", "gru1_input_weights_int8","gru1_input_weights_float", "gru1_input_weights_idx", NULL, "gru1_input_scale", 384, 1152)) return 1;
    if (linear_init(&model->gru1_recurrent, arrays, "gru1_recurrent_bias", "gru1_recurrent_subias", "gru1_recurrent_weights_int8","gru1_recurrent_weights_float", "gru1_recurrent_weights_idx", "gru1_recurrent_weights_diag", "gru1_recurrent_scale", 384, 1152)) return 1;
    if (linear_init(&model->gru2_input, arrays, "gru2_input_bias", "gru2_input_subias", "gru2_input_weights_int8","gru2_input_weights_float", "gru2_input_weights_idx", NULL, "gru2_input_scale", 384, 1152)) return 1;
    if (linear_init(&model->gru2_recurrent, arrays, "gru2_recurrent_bias", "gru2_recurrent_subias", "gru2_recurrent_weights_int8","gru2_recurrent_weights_float", "gru2_recurrent_weights_idx", "gru2_recurrent_weights_diag", "gru2_recurrent_scale", 384, 1152)) return 1;
    if (linear_init(&model->gru3_input, arrays, "gru3_input_bias", "gru3_input_subias", "gru3_input_weights_int8","gru3_input_weights_float", "gru3_input_weights_idx", NULL, "gru3_input_scale", 384, 1152)) return 1;
    if (linear_init(&model->gru3_recurrent, arrays, "gru3_recurrent_bias", "gru3_recurrent_subias", "gru3_recurrent_weights_int8","gru3_recurrent_weights_float", "gru3_recurrent_weights_idx", "gru3_recurrent_weights_diag", "gru3_recurrent_scale", 384, 1152)) return 1;
    if (linear_init(&model->dense_out, arrays, "dense_out_bias", NULL, NULL,"dense_out_weights_float", NULL, NULL, NULL, 1536, 32)) return 1;
    if (linear_init(&model->vad_dense, arrays, "vad_dense_bias", NULL, NULL,"vad_dense_weights_float", NULL, NULL, NULL, 1536, 1)) return 1;
    return 0;
}
#endif /* DUMP_BINARY_WEIGHTS */
