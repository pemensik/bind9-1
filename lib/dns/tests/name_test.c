/*
 * Copyright (C) 2014-2017  Internet Systems Consortium, Inc. ("ISC")
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* $Id$ */

/*! \file */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/mem.h>
#include <isc/os.h>
#include <isc/print.h>
#include <isc/thread.h>
#include <isc/util.h>

#include <dns/compress.h>
#include <dns/name.h>
#include <dns/fixedname.h>

#include "dnstest.h"

/*
 * Individual unit tests
 */

ATF_TC(fullcompare);
ATF_TC_HEAD(fullcompare, tc) {
	atf_tc_set_md_var(tc, "descr", "dns_name_fullcompare test");
}
ATF_TC_BODY(fullcompare, tc) {
	dns_fixedname_t fixed1;
	dns_fixedname_t fixed2;
	dns_name_t *name1;
	dns_name_t *name2;
	dns_namereln_t relation;
	int i;
	isc_result_t result;
	struct {
		const char *name1;
		const char *name2;
		dns_namereln_t relation;
		int order;
		unsigned int nlabels;
	} data[] = {
		/* relative */
		{ "", "", dns_namereln_equal, 0, 0 },
		{ "foo", "", dns_namereln_subdomain, 1, 0 },
		{ "", "foo", dns_namereln_contains, -1, 0 },
		{ "foo", "bar", dns_namereln_none, 4, 0 },
		{ "bar", "foo", dns_namereln_none, -4, 0 },
		{ "bar.foo", "foo", dns_namereln_subdomain, 1, 1 },
		{ "foo", "bar.foo", dns_namereln_contains, -1, 1 },
		{ "baz.bar.foo", "bar.foo", dns_namereln_subdomain, 1, 2 },
		{ "bar.foo", "baz.bar.foo", dns_namereln_contains, -1, 2 },
		{ "foo.example", "bar.example", dns_namereln_commonancestor,
		  4, 1 },

		/* absolute */
		{ ".", ".", dns_namereln_equal, 0, 1 },
		{ "foo.", "bar.", dns_namereln_commonancestor, 4, 1 },
		{ "bar.", "foo.", dns_namereln_commonancestor, -4, 1 },
		{ "foo.example.", "bar.example.", dns_namereln_commonancestor,
		  4, 2 },
		{ "bar.foo.", "foo.", dns_namereln_subdomain, 1, 2 },
		{ "foo.", "bar.foo.", dns_namereln_contains, -1, 2 },
		{ "baz.bar.foo.", "bar.foo.", dns_namereln_subdomain, 1, 3 },
		{ "bar.foo.", "baz.bar.foo.", dns_namereln_contains, -1, 3 },
		{ NULL, NULL, dns_namereln_none, 0, 0 }
	};

	UNUSED(tc);

	dns_fixedname_init(&fixed1);
	name1 = dns_fixedname_name(&fixed1);
	dns_fixedname_init(&fixed2);
	name2 = dns_fixedname_name(&fixed2);
	for (i = 0; data[i].name1 != NULL; i++) {
		int order = 3000;
		unsigned int nlabels = 3000;

		if (data[i].name1[0] == 0) {
			dns_fixedname_init(&fixed1);
		} else {
			result = dns_name_fromstring2(name1, data[i].name1,
						      NULL, 0, NULL);
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		}
		if (data[i].name2[0] == 0) {
			dns_fixedname_init(&fixed2);
		} else {
			result = dns_name_fromstring2(name2, data[i].name2,
						      NULL, 0, NULL);
			ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		}
		relation = dns_name_fullcompare(name1, name1, &order, &nlabels);
		ATF_REQUIRE_EQ(relation, dns_namereln_equal);
		ATF_REQUIRE_EQ(order, 0);
		ATF_REQUIRE_EQ(nlabels, name1->labels);

		/* Some random initializer */
		order = 3001;
		nlabels = 3001;

		relation = dns_name_fullcompare(name1, name2, &order, &nlabels);
		ATF_REQUIRE_EQ(relation, data[i].relation);
		ATF_REQUIRE_EQ(order, data[i].order);
		ATF_REQUIRE_EQ(nlabels, data[i].nlabels);
	}
}

static void
compress_test(dns_name_t *name1, dns_name_t *name2, dns_name_t *name3,
	      unsigned char *expected, unsigned int length,
	      dns_compress_t *cctx, dns_decompress_t *dctx)
{
	isc_buffer_t source;
	isc_buffer_t target;
	dns_name_t name;
	unsigned char buf1[1024];
	unsigned char buf2[1024];

	isc_buffer_init(&source, buf1, sizeof(buf1));
	isc_buffer_init(&target, buf2, sizeof(buf2));

	ATF_REQUIRE_EQ(dns_name_towire(name1, cctx, &source), ISC_R_SUCCESS);

	ATF_CHECK_EQ(dns_name_towire(name2, cctx, &source), ISC_R_SUCCESS);
	ATF_CHECK_EQ(dns_name_towire(name2, cctx, &source), ISC_R_SUCCESS);
	ATF_CHECK_EQ(dns_name_towire(name3, cctx, &source), ISC_R_SUCCESS);

	isc_buffer_setactive(&source, source.used);

	dns_name_init(&name, NULL);
	RUNTIME_CHECK(dns_name_fromwire(&name, &source, dctx, ISC_FALSE,
					&target) == ISC_R_SUCCESS);
	RUNTIME_CHECK(dns_name_fromwire(&name, &source, dctx, ISC_FALSE,
					&target) == ISC_R_SUCCESS);
	RUNTIME_CHECK(dns_name_fromwire(&name, &source, dctx, ISC_FALSE,
					&target) == ISC_R_SUCCESS);
	RUNTIME_CHECK(dns_name_fromwire(&name, &source, dctx, ISC_FALSE,
					&target) == ISC_R_SUCCESS);
	dns_decompress_invalidate(dctx);

	ATF_CHECK_EQ(target.used, length);
	ATF_CHECK(memcmp(target.base, expected, target.used) == 0);
}

ATF_TC(compression);
ATF_TC_HEAD(compression, tc) {
	atf_tc_set_md_var(tc, "descr", "name compression test");
}
ATF_TC_BODY(compression, tc) {
	unsigned int allowed;
	dns_compress_t cctx;
	dns_decompress_t dctx;
	dns_name_t name1;
	dns_name_t name2;
	dns_name_t name3;
	isc_region_t r;
	unsigned char plain1[] = "\003yyy\003foo";
	unsigned char plain2[] = "\003bar\003yyy\003foo";
	unsigned char plain3[] = "\003xxx\003bar\003foo";
	unsigned char plain[] = "\003yyy\003foo\0\003bar\003yyy\003foo\0\003"
				"bar\003yyy\003foo\0\003xxx\003bar\003foo";

	ATF_REQUIRE_EQ(dns_test_begin(NULL, ISC_FALSE), ISC_R_SUCCESS);;

	dns_name_init(&name1, NULL);
	r.base = plain1;
	r.length = sizeof(plain1);
	dns_name_fromregion(&name1, &r);

	dns_name_init(&name2, NULL);
	r.base = plain2;
	r.length = sizeof(plain2);
	dns_name_fromregion(&name2, &r);

	dns_name_init(&name3, NULL);
	r.base = plain3;
	r.length = sizeof(plain3);
	dns_name_fromregion(&name3, &r);

	/* Test 1: NONE */
	allowed = DNS_COMPRESS_NONE;
	ATF_REQUIRE_EQ(dns_compress_init(&cctx, -1, mctx), ISC_R_SUCCESS);
	dns_compress_setmethods(&cctx, allowed);
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_STRICT);
	dns_decompress_setmethods(&dctx, allowed);

	compress_test(&name1, &name2, &name3, plain, sizeof(plain),
		      &cctx, &dctx);

	dns_compress_rollback(&cctx, 0);
	dns_compress_invalidate(&cctx);

	/* Test2: GLOBAL14 */
	allowed = DNS_COMPRESS_GLOBAL14;
	ATF_REQUIRE_EQ(dns_compress_init(&cctx, -1, mctx), ISC_R_SUCCESS);
	dns_compress_setmethods(&cctx, allowed);
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_STRICT);
	dns_decompress_setmethods(&dctx, allowed);

	compress_test(&name1, &name2, &name3, plain, sizeof(plain),
		      &cctx, &dctx);

	dns_compress_rollback(&cctx, 0);
	dns_compress_invalidate(&cctx);

	/* Test3: ALL */
	allowed = DNS_COMPRESS_ALL;
	ATF_REQUIRE_EQ(dns_compress_init(&cctx, -1, mctx), ISC_R_SUCCESS);
	dns_compress_setmethods(&cctx, allowed);
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_STRICT);
	dns_decompress_setmethods(&dctx, allowed);

	compress_test(&name1, &name2, &name3, plain, sizeof(plain),
		      &cctx, &dctx);

	dns_compress_rollback(&cctx, 0);
	dns_compress_invalidate(&cctx);

	/* Test4: NONE disabled */
	allowed = DNS_COMPRESS_NONE;
	ATF_REQUIRE_EQ(dns_compress_init(&cctx, -1, mctx), ISC_R_SUCCESS);
	dns_compress_setmethods(&cctx, allowed);
	dns_compress_disable(&cctx);
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_STRICT);
	dns_decompress_setmethods(&dctx, allowed);

	compress_test(&name1, &name2, &name3, plain, sizeof(plain),
		      &cctx, &dctx);

	dns_compress_rollback(&cctx, 0);
	dns_compress_invalidate(&cctx);

	/* Test5: GLOBAL14 disabled */
	allowed = DNS_COMPRESS_GLOBAL14;
	ATF_REQUIRE_EQ(dns_compress_init(&cctx, -1, mctx), ISC_R_SUCCESS);
	dns_compress_setmethods(&cctx, allowed);
	dns_compress_disable(&cctx);
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_STRICT);
	dns_decompress_setmethods(&dctx, allowed);

	compress_test(&name1, &name2, &name3, plain, sizeof(plain),
		      &cctx, &dctx);

	dns_compress_rollback(&cctx, 0);
	dns_compress_invalidate(&cctx);

	/* Test6: ALL disabled */
	allowed = DNS_COMPRESS_ALL;
	ATF_REQUIRE_EQ(dns_compress_init(&cctx, -1, mctx), ISC_R_SUCCESS);
	dns_compress_setmethods(&cctx, allowed);
	dns_compress_disable(&cctx);
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_STRICT);
	dns_decompress_setmethods(&dctx, allowed);

	compress_test(&name1, &name2, &name3, plain, sizeof(plain),
		      &cctx, &dctx);

	dns_compress_rollback(&cctx, 0);
	dns_compress_invalidate(&cctx);

	dns_test_end();
}

ATF_TC(istat);
ATF_TC_HEAD(istat, tc) {
	atf_tc_set_md_var(tc, "descr", "is trust-anchor-telementry test");
}
ATF_TC_BODY(istat, tc) {
	dns_fixedname_t fixed;
	dns_name_t *name;
	isc_result_t result;
	size_t i;
	struct {
		const char *name;
		isc_boolean_t istat;
	} data[] = {
		{ ".", ISC_FALSE },
		{ "_ta-", ISC_FALSE },
		{ "_ta-1234", ISC_TRUE },
		{ "_TA-1234", ISC_TRUE },
		{ "+TA-1234", ISC_FALSE },
		{ "_fa-1234", ISC_FALSE },
		{ "_td-1234", ISC_FALSE },
		{ "_ta_1234", ISC_FALSE },
		{ "_ta-g234", ISC_FALSE },
		{ "_ta-1h34", ISC_FALSE },
		{ "_ta-12i4", ISC_FALSE },
		{ "_ta-123j", ISC_FALSE },
		{ "_ta-1234-abcf", ISC_TRUE },
		{ "_ta-1234-abcf-ED89", ISC_TRUE },
		{ "_ta-12345-abcf-ED89", ISC_FALSE },
		{ "_ta-.example", ISC_FALSE },
		{ "_ta-1234.example", ISC_TRUE },
		{ "_ta-1234-abcf.example", ISC_TRUE },
		{ "_ta-1234-abcf-ED89.example", ISC_TRUE },
		{ "_ta-12345-abcf-ED89.example", ISC_FALSE },
		{ "_ta-1234-abcfe-ED89.example", ISC_FALSE },
		{ "_ta-1234-abcf-EcD89.example", ISC_FALSE }
	};

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);

	for (i = 0; i < sizeof(data)/sizeof(data[0]); i++) {
		result = dns_name_fromstring(name, data[i].name, 0, NULL);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
		ATF_CHECK_EQ_MSG(dns_name_istat(name), data[i].istat,
				 "testing %s - expected %u", data[i].name, data[i].istat);
	}
}

#ifdef ISC_PLATFORM_USETHREADS
#ifdef DNS_BENCHMARK_TESTS

/*
 * XXXMUKS: Don't delete this code. It is useful in benchmarking the
 * name parser, but we don't require it as part of the unit test runs.
 */

ATF_TC(benchmark);
ATF_TC_HEAD(benchmark, tc) {
	atf_tc_set_md_var(tc, "descr",
			  "Benchmark dns_name_fromwire() implementation");
}

static void *
fromwire_thread(void *arg) {
	unsigned int maxval = 32000000;
	uint8_t data[] = {
		3, 'w', 'w', 'w',
		7, 'e', 'x', 'a', 'm', 'p', 'l', 'e',
		7, 'i', 'n', 'v', 'a', 'l', 'i', 'd',
		0
	};
	unsigned char output_data[DNS_NAME_MAXWIRE];
	isc_buffer_t source, target;
	unsigned int i;
	dns_decompress_t dctx;

	UNUSED(arg);

	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_STRICT);
	dns_decompress_setmethods(&dctx, DNS_COMPRESS_NONE);

	isc_buffer_init(&source, data, sizeof(data));
	isc_buffer_add(&source, sizeof(data));
	isc_buffer_init(&target, output_data, sizeof(output_data));

	/* Parse 32 million names in each thread */
	for (i = 0; i < maxval; i++) {
		dns_name_t name;

		isc_buffer_clear(&source);
		isc_buffer_clear(&target);
		isc_buffer_add(&source, sizeof(data));
		isc_buffer_setactive(&source, sizeof(data));

		dns_name_init(&name, NULL);
		(void) dns_name_fromwire(&name, &source, &dctx, 0, &target);
	}

	return (NULL);
}

ATF_TC_BODY(benchmark, tc) {
	isc_result_t result;
	unsigned int i;
	isc_time_t ts1, ts2;
	double t;
	unsigned int nthreads;
	isc_thread_t threads[32];

	UNUSED(tc);

	debug_mem_record = ISC_FALSE;

	result = dns_test_begin(NULL, ISC_TRUE);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	result = isc_time_now(&ts1);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	nthreads = ISC_MIN(isc_os_ncpus(), 32);
	nthreads = ISC_MAX(nthreads, 1);
	for (i = 0; i < nthreads; i++) {
		result = isc_thread_create(fromwire_thread, NULL, &threads[i]);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	}

	for (i = 0; i < nthreads; i++) {
		result = isc_thread_join(threads[i], NULL);
		ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);
	}

	result = isc_time_now(&ts2);
	ATF_REQUIRE_EQ(result, ISC_R_SUCCESS);

	t = isc_time_microdiff(&ts2, &ts1);

	printf("%u dns_name_fromwire() calls, %f seconds, %f calls/second\n",
	       nthreads * 32000000, t / 1000000.0,
	       (nthreads * 32000000) / (t / 1000000.0));

	dns_test_end();
}

#endif /* DNS_BENCHMARK_TESTS */
#endif /* ISC_PLATFORM_USETHREADS */

/*
 * Main
 */
ATF_TP_ADD_TCS(tp) {
	ATF_TP_ADD_TC(tp, fullcompare);
	ATF_TP_ADD_TC(tp, compression);
	ATF_TP_ADD_TC(tp, istat);
#ifdef ISC_PLATFORM_USETHREADS
#ifdef DNS_BENCHMARK_TESTS
	ATF_TP_ADD_TC(tp, benchmark);
#endif /* DNS_BENCHMARK_TESTS */
#endif /* ISC_PLATFORM_USETHREADS */

	return (atf_no_error());
}
