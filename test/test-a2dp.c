/*
 * test-a2dp.c
 * Copyright (c) 2016-2022 Arkadiusz Bokowy
 *
 * This file is a part of bluez-alsa.
 *
 * This project is licensed under the terms of the MIT license.
 *
 */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <check.h>
#include <glib.h>

#include "a2dp.h"
#include "a2dp-sbc.h"
#include "bluealsa-config.h"
#include "codec-sbc.h"
#include "shared/a2dp-codecs.h"
#include "shared/log.h"

bool ba_transport_pcm_is_active(struct ba_transport_pcm *pcm) { (void)pcm; return false; }
int ba_transport_pcm_release(struct ba_transport_pcm *pcm) { (void)pcm; return -1; }
int ba_transport_stop_if_no_clients(struct ba_transport *t) { (void)t; return -1; }
int ba_transport_thread_bt_release(struct ba_transport_thread *th) { (void)th; return -1; }
int ba_transport_thread_create(struct ba_transport_thread *th,
		void *(*routine)(struct ba_transport_thread *), const char *name, bool master) {
	(void)th; (void)routine; (void)name; (void)master; return -1; }
int ba_transport_thread_set_state(struct ba_transport_thread *th,
		enum ba_transport_thread_state state, bool force) {
	(void)th; (void)state; (void)force; return -1; }
int ba_transport_thread_signal_recv(struct ba_transport_thread *th,
		enum ba_transport_thread_signal *signal) {
	(void)th; (void)signal; return -1; }
void ba_transport_thread_cleanup(struct ba_transport_thread *th) { (void)th; }

START_TEST(test_a2dp_codecs_codec_id_from_string) {
	ck_assert_int_eq(a2dp_codecs_codec_id_from_string("SBC"), A2DP_CODEC_SBC);
	ck_assert_int_eq(a2dp_codecs_codec_id_from_string("apt-x"), A2DP_CODEC_VENDOR_APTX);
	ck_assert_int_eq(a2dp_codecs_codec_id_from_string("unknown"), 0xFFFF);
} END_TEST

START_TEST(test_a2dp_codecs_codec_id_to_string) {
	ck_assert_str_eq(a2dp_codecs_codec_id_to_string(A2DP_CODEC_SBC), "SBC");
	ck_assert_str_eq(a2dp_codecs_codec_id_to_string(A2DP_CODEC_VENDOR_APTX), "aptX");
	ck_assert_ptr_eq(a2dp_codecs_codec_id_to_string(0xFFFF), NULL);
} END_TEST

START_TEST(test_a2dp_codecs_get_canonical_name) {
	ck_assert_str_eq(a2dp_codecs_get_canonical_name("apt-x"), "aptX");
	ck_assert_str_eq(a2dp_codecs_get_canonical_name("Foo-Bar"), "Foo-Bar");
} END_TEST

START_TEST(test_a2dp_dir) {
	ck_assert_int_eq(A2DP_SOURCE, !A2DP_SINK);
	ck_assert_int_eq(!A2DP_SOURCE, A2DP_SINK);
} END_TEST

START_TEST(test_a2dp_codec_cmp) {

	struct a2dp_codec codec1 = { .dir = A2DP_SOURCE, .codec_id = A2DP_CODEC_SBC };
	struct a2dp_codec codec2 = { .dir = A2DP_SOURCE, .codec_id = A2DP_CODEC_MPEG24 };
	struct a2dp_codec codec3 = { .dir = A2DP_SOURCE, .codec_id = A2DP_CODEC_VENDOR_APTX };
	struct a2dp_codec codec4 = { .dir = A2DP_SINK, .codec_id = A2DP_CODEC_SBC };
	struct a2dp_codec codec5 = { .dir = A2DP_SINK, .codec_id = A2DP_CODEC_VENDOR_APTX };

	struct a2dp_codec * codecs[] = { &codec3, &codec1, &codec4, &codec5, &codec2 };
	qsort(codecs, ARRAYSIZE(codecs), sizeof(*codecs), QSORT_COMPAR(a2dp_codec_ptr_cmp));

	ck_assert_ptr_eq(codecs[0], &codec1);
	ck_assert_ptr_eq(codecs[1], &codec2);
	ck_assert_ptr_eq(codecs[2], &codec3);
	ck_assert_ptr_eq(codecs[3], &codec4);
	ck_assert_ptr_eq(codecs[4], &codec5);

} END_TEST

START_TEST(test_a2dp_sep_cmp) {

	struct a2dp_sep seps[] = {
		{ .dir = A2DP_SOURCE, .codec_id = A2DP_CODEC_VENDOR_APTX },
		{ .dir = A2DP_SINK, .codec_id = A2DP_CODEC_SBC },
		{ .dir = A2DP_SINK, .codec_id = A2DP_CODEC_VENDOR_APTX },
		{ .dir = A2DP_SOURCE, .codec_id = A2DP_CODEC_MPEG24 },
		{ .dir = A2DP_SOURCE, .codec_id = A2DP_CODEC_SBC } };
	qsort(seps, ARRAYSIZE(seps), sizeof(*seps), QSORT_COMPAR(a2dp_sep_cmp));

	ck_assert_int_eq(seps[0].codec_id, A2DP_CODEC_SBC);
	ck_assert_int_eq(seps[1].codec_id, A2DP_CODEC_MPEG24);
	ck_assert_int_eq(seps[2].codec_id, A2DP_CODEC_VENDOR_APTX);
	ck_assert_int_eq(seps[3].dir, A2DP_SINK);
	ck_assert_int_eq(seps[3].codec_id, A2DP_CODEC_SBC);
	ck_assert_int_eq(seps[4].dir, A2DP_SINK);
	ck_assert_int_eq(seps[4].codec_id, A2DP_CODEC_VENDOR_APTX);

} END_TEST

START_TEST(test_a2dp_codec_lookup) {
	ck_assert_ptr_eq(a2dp_codec_lookup(A2DP_CODEC_SBC, A2DP_SOURCE), &a2dp_sbc_source);
	ck_assert_ptr_eq(a2dp_codec_lookup(0xFFFF, A2DP_SOURCE), NULL);
} END_TEST

START_TEST(test_a2dp_get_vendor_codec_id) {

	uint8_t cfg0[4] = { 0xDE, 0xAD, 0xB0, 0xBE };
	ck_assert_int_eq(a2dp_get_vendor_codec_id(cfg0, sizeof(cfg0)), 0xFFFF);
	ck_assert_int_eq(errno, EINVAL);

	a2dp_aptx_t cfg1 = { A2DP_SET_VENDOR_ID_CODEC_ID(APTX_VENDOR_ID, APTX_CODEC_ID), 0, 0 };
	ck_assert_int_eq(a2dp_get_vendor_codec_id(&cfg1, sizeof(cfg1)), A2DP_CODEC_VENDOR_APTX);

	a2dp_aptx_t cfg2 = { A2DP_SET_VENDOR_ID_CODEC_ID(APTX_VENDOR_ID, 0x69), 0, 0 };
	ck_assert_int_eq(a2dp_get_vendor_codec_id(&cfg2, sizeof(cfg2)), 0xFFFF);
	ck_assert_int_eq(errno, ENOTSUP);

} END_TEST

START_TEST(test_a2dp_check_configuration) {

	const a2dp_sbc_t cfg_valid = {
		.frequency = SBC_SAMPLING_FREQ_44100,
		.channel_mode = SBC_CHANNEL_MODE_STEREO,
		.block_length = SBC_BLOCK_LENGTH_8,
		.subbands = SBC_SUBBANDS_8,
		.allocation_method = SBC_ALLOCATION_SNR,
		.min_bitpool = 42,
		.max_bitpool = 62,
	};

	ck_assert_int_eq(a2dp_check_configuration(&a2dp_sbc_source,
				&cfg_valid, sizeof(cfg_valid)), A2DP_CHECK_OK);

	const a2dp_sbc_t cfg_invalid = {
		.frequency = SBC_SAMPLING_FREQ_16000 | SBC_SAMPLING_FREQ_44100,
		.channel_mode = SBC_CHANNEL_MODE_STEREO | SBC_CHANNEL_MODE_JOINT_STEREO,
		.block_length = SBC_BLOCK_LENGTH_8,
		.allocation_method = SBC_ALLOCATION_SNR,
	};

	ck_assert_int_eq(a2dp_check_configuration(&a2dp_sbc_source,
				&cfg_invalid, sizeof(cfg_invalid)),
			A2DP_CHECK_ERR_SAMPLING | A2DP_CHECK_ERR_CHANNELS | A2DP_CHECK_ERR_SBC_SUB_BANDS);

} END_TEST

START_TEST(test_a2dp_filter_capabilities) {

	a2dp_sbc_t cfg = {
		.frequency = SBC_SAMPLING_FREQ_44100,
		.channel_mode = SBC_CHANNEL_MODE_MONO | SBC_CHANNEL_MODE_STEREO,
		.block_length = SBC_BLOCK_LENGTH_4 | SBC_BLOCK_LENGTH_8,
		.subbands = SBC_SUBBANDS_4,
		.allocation_method = SBC_ALLOCATION_SNR,
		.min_bitpool = 42,
		.max_bitpool = 255,
	};

	hexdump("Capabilities original", &cfg, sizeof(cfg), true);
	ck_assert_int_eq(a2dp_filter_capabilities(&a2dp_sbc_source, &cfg, sizeof(cfg)), 0);

	hexdump("Capabilities filtered", &cfg, sizeof(cfg), true);
	ck_assert_int_eq(cfg.frequency, SBC_SAMPLING_FREQ_44100);
	ck_assert_int_eq(cfg.channel_mode, SBC_CHANNEL_MODE_MONO | SBC_CHANNEL_MODE_STEREO);
	ck_assert_int_eq(cfg.block_length, SBC_BLOCK_LENGTH_4 | SBC_BLOCK_LENGTH_8);
	ck_assert_int_eq(cfg.subbands, SBC_SUBBANDS_4);
	ck_assert_int_eq(cfg.allocation_method, SBC_ALLOCATION_SNR);
	ck_assert_int_eq(cfg.min_bitpool, MAX(SBC_MIN_BITPOOL, 42));
	ck_assert_int_eq(cfg.max_bitpool, MIN(SBC_MAX_BITPOOL, 255));

} END_TEST

START_TEST(test_a2dp_select_configuration) {

	a2dp_sbc_t cfg;
	const a2dp_sbc_t cfg_ = {
		.frequency = SBC_SAMPLING_FREQ_16000 | SBC_SAMPLING_FREQ_44100 | SBC_SAMPLING_FREQ_48000,
		.channel_mode = SBC_CHANNEL_MODE_MONO | SBC_CHANNEL_MODE_DUAL_CHANNEL | SBC_CHANNEL_MODE_STEREO,
		.block_length = SBC_BLOCK_LENGTH_4 | SBC_BLOCK_LENGTH_8,
		.subbands = SBC_SUBBANDS_4 | SBC_SUBBANDS_8,
		.allocation_method = SBC_ALLOCATION_SNR | SBC_ALLOCATION_LOUDNESS,
		.min_bitpool = 42,
		.max_bitpool = 255,
	};

	cfg = cfg_;
	ck_assert_int_eq(a2dp_select_configuration(&a2dp_sbc_source, &cfg, sizeof(cfg) + 1), -1);
	ck_assert_int_eq(errno, EINVAL);

	cfg = cfg_;
	ck_assert_int_eq(a2dp_select_configuration(&a2dp_sbc_source, &cfg, sizeof(cfg)), 0);
	ck_assert_int_eq(cfg.frequency, SBC_SAMPLING_FREQ_48000);
	ck_assert_int_eq(cfg.channel_mode, SBC_CHANNEL_MODE_STEREO);
	ck_assert_int_eq(cfg.block_length, SBC_BLOCK_LENGTH_8);
	ck_assert_int_eq(cfg.subbands, SBC_SUBBANDS_8);
	ck_assert_int_eq(cfg.allocation_method, SBC_ALLOCATION_LOUDNESS);
	ck_assert_int_eq(cfg.min_bitpool, 42);
	ck_assert_int_eq(cfg.max_bitpool, 250);

	cfg = cfg_;
	config.a2dp.force_44100 = true;
	config.sbc_quality = SBC_QUALITY_XQ;
	ck_assert_int_eq(a2dp_select_configuration(&a2dp_sbc_source, &cfg, sizeof(cfg)), 0);
	ck_assert_int_eq(cfg.frequency, SBC_SAMPLING_FREQ_44100);
	ck_assert_int_eq(cfg.channel_mode, SBC_CHANNEL_MODE_DUAL_CHANNEL);
	ck_assert_int_eq(cfg.block_length, SBC_BLOCK_LENGTH_8);
	ck_assert_int_eq(cfg.subbands, SBC_SUBBANDS_8);
	ck_assert_int_eq(cfg.allocation_method, SBC_ALLOCATION_LOUDNESS);
	ck_assert_int_eq(cfg.min_bitpool, 42);
	ck_assert_int_eq(cfg.max_bitpool, 250);

} END_TEST

int main(void) {

	Suite *s = suite_create(__FILE__);
	TCase *tc = tcase_create(__FILE__);
	SRunner *sr = srunner_create(s);

	suite_add_tcase(s, tc);

	tcase_add_test(tc, test_a2dp_codecs_codec_id_from_string);
	tcase_add_test(tc, test_a2dp_codecs_codec_id_to_string);
	tcase_add_test(tc, test_a2dp_codecs_get_canonical_name);

	tcase_add_test(tc, test_a2dp_dir);
	tcase_add_test(tc, test_a2dp_codec_cmp);
	tcase_add_test(tc, test_a2dp_sep_cmp);
	tcase_add_test(tc, test_a2dp_codec_lookup);
	tcase_add_test(tc, test_a2dp_get_vendor_codec_id);
	tcase_add_test(tc, test_a2dp_check_configuration);
	tcase_add_test(tc, test_a2dp_filter_capabilities);
	tcase_add_test(tc, test_a2dp_select_configuration);

	srunner_run_all(sr, CK_ENV);
	int nf = srunner_ntests_failed(sr);
	srunner_free(sr);

	return nf == 0 ? 0 : 1;
}
