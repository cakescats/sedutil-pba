/* Host-side tests for the parts of sedutil/linuxpba that don't need a drive.
 * Builds on Linux and macOS: make -C tests/host check */
#include "os.h"
#include <cstdio>
#include <fstream>
#include "DtaOptions.h"
#include "DtaHashPwd.h"
#include "DtaDev.h"
#include "PbaConfig.h"

sedutiloutput outputFormat = sedutilNormal;

/* DtaHashPwd only needs the serial number from the device */
static char testSerial[21] = "S5GXNF0R123456      ";
char *DtaDev::getSerialNum() { return testSerial; }

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static std::string hex(const std::vector<uint8_t> &h)
{
	std::string s;
	char b[3];
	for (size_t i = 2; i < h.size(); i++) { snprintf(b, sizeof(b), "%02x", h[i]); s += b; }
	return s;
}

static void testHashSpec()
{
	DtaHashAlg alg;
	unsigned int iter;
	CHECK(!DtaParseHashSpec("sha1", &alg, &iter) && alg == DTA_HASH_SHA1 && iter == 75000);
	CHECK(!DtaParseHashSpec("SHA512", &alg, &iter) && alg == DTA_HASH_SHA512 && iter == 500000);
	CHECK(!DtaParseHashSpec("sha512:1000", &alg, &iter) && alg == DTA_HASH_SHA512 && iter == 1000);
	CHECK(DtaParseHashSpec("sha256", &alg, &iter));
	CHECK(DtaParseHashSpec("sha1:", &alg, &iter));
	CHECK(DtaParseHashSpec("sha1:0", &alg, &iter));
	CHECK(DtaParseHashSpec("sha1:12x", &alg, &iter));
	CHECK(DtaParseHashSpec("sha1x", &alg, &iter));
}

/* Expected values come from Python hashlib.pbkdf2_hmac with the same
 * serial number salt, see tests/host/vectors.py */
static void testDriveHash(const char *expectedSha1, const char *expectedSha512)
{
	DtaDev *d = (DtaDev *)calloc(1, 4096); /* only no_hash_passwords is read */
	std::vector<uint8_t> h;
	DtaSetHashConfig(DTA_HASH_SHA1, 75000);
	DtaHashPwd(h, (char *)"correct horse", d);
	CHECK(h.size() == 34 && h[0] == 0xd0 && h[1] == 32);
	CHECK(hex(h) == expectedSha1);
	DtaSetHashConfig(DTA_HASH_SHA512, 1000);
	DtaHashPwd(h, (char *)"correct horse", d);
	CHECK(hex(h) == expectedSha512);
	DtaSetHashConfig(DTA_HASH_SHA1, 75000);
	free(d);
}

static void testConfig()
{
	const char *conf = "test-pba.conf";
	const char *cmdline = "test-cmdline";
	std::ofstream(conf) << "# comment\n"
		"timeout = 7\n"
		"hash = sha512:1000, sha1\n"
		"service = password\n"
		"service_hash = sha512:10:00:00\n"
		"title = \"Hello PBA\"\n"
		"bogus = 1\n";
	std::ofstream(cmdline) << "BOOT_IMAGE=/bzImage loglevel=0 cakes.timeout=-1 cakes.reboot=cold "
		"cakes.service=on cakes.mode=service cakes.title=\"From cmdline\"\n";
	PbaConfig cfg;
	PbaLoadConfig(cfg, conf, cmdline);
	CHECK(cfg.timeout == -1);
	CHECK(cfg.reboot == "cold");
	CHECK(cfg.hashes.size() == 2);
	CHECK(cfg.hashes[0].alg == DTA_HASH_SHA512 && cfg.hashes[0].iter == 1000);
	CHECK(cfg.hashes[1].alg == DTA_HASH_SHA1 && cfg.hashes[1].iter == 75000);
	CHECK(cfg.service == PBA_SERVICE_PASSWORD); /* cmdline may not change it */
	CHECK(cfg.service_boot);
	CHECK(cfg.title == "From cmdline");
	CHECK(cfg.warnings.size() == 2); /* bogus, cakes.service */

	std::ofstream(conf) << "service = password\ntimeout = abc\n";
	std::ofstream(cmdline) << "cakes.mode=service\n";
	PbaConfig cfg2;
	PbaLoadConfig(cfg2, conf, cmdline);
	CHECK(cfg2.service == PBA_SERVICE_OFF); /* no service_hash */
	CHECK(!cfg2.service_boot);
	CHECK(cfg2.timeout == 3);
	remove(conf);
	remove(cmdline);
}

static void testServicePassword(const char *serviceHash)
{
	PbaConfig cfg;
	cfg.service_hash = serviceHash;
	CHECK(PbaCheckServicePassword(cfg, "svc-pass"));
	CHECK(!PbaCheckServicePassword(cfg, "svc-pasS"));
	CHECK(!PbaCheckServicePassword(cfg, ""));
	cfg.service_hash = "sha1:1000:00:00";
	CHECK(!PbaCheckServicePassword(cfg, "svc-pass"));
	cfg.service_hash = "garbage";
	CHECK(!PbaCheckServicePassword(cfg, "svc-pass"));
}

int main(int argc, char **argv)
{
	if (argc != 4) {
		printf("usage: %s <sha1 hex> <sha512 hex> <service_hash>\n", argv[0]);
		return 2;
	}
	CLog::Level() = CLog::FromInt(2);
	RCLog::Level() = RCLog::FromInt(2);
	TestPBKDF2();
	testHashSpec();
	testDriveHash(argv[1], argv[2]);
	testConfig();
	testServicePassword(argv[3]);
	printf(failures ? "%d host test(s) FAILED\n" : "host tests passed\n", failures);
	return failures ? 1 : 0;
}
