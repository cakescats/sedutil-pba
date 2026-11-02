/* C:B**************************************************************************
This software is Copyright 2014-2017 Bright Plaza Inc. <drivetrust@drivetrust.com>

This file is part of sedutil.

sedutil is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

sedutil is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with sedutil.  If not, see <http://www.gnu.org/licenses/>.

 * C:E********************************************************************** */
#include "os.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include "DtaHashPwd.h"
#include "DtaLexicon.h"
#include "DtaOptions.h"
#include "DtaDev.h"
#include "log.h"

extern "C" {
#include "pbkdf2.h"
#include "sha1.h"
#include "sha2.h"
}
using namespace std;

static DtaHashAlg hashAlg = DTA_HASH_SHA1;
static unsigned int hashIter = 75000;

void DtaSetHashConfig(DtaHashAlg alg, unsigned int iter)
{
	hashAlg = alg;
	hashIter = iter;
}

void DtaGetHashConfig(DtaHashAlg *alg, unsigned int *iter)
{
	*alg = hashAlg;
	*iter = hashIter;
}

int DtaParseHashSpec(const char *spec, DtaHashAlg *alg, unsigned int *iter)
{
	const char *colon = strchr(spec, ':');
	size_t namelen = colon ? (size_t)(colon - spec) : strlen(spec);
	if ((4 == namelen) && !strncasecmp(spec, "sha1", 4)) {
		*alg = DTA_HASH_SHA1;
		*iter = 75000;
	}
	else if ((6 == namelen) && !strncasecmp(spec, "sha512", 6)) {
		*alg = DTA_HASH_SHA512;
		*iter = 500000;
	}
	else return 1;
	if (colon) {
		char *end;
		unsigned long n = strtoul(colon + 1, &end, 10);
		if ((end == colon + 1) || *end || (0 == n) || (n > 100000000UL)) return 1;
		*iter = (unsigned int)n;
	}
	return 0;
}

void DtaHashPassword(vector<uint8_t> &hash, char * password, vector<uint8_t> salt,
	unsigned int iter, uint8_t hashsize, DtaHashAlg alg)
{
	LOG(D1) << " Entered DtaHashPassword";
	// if the hashsize can be > 255 the token overhead logic needs to be fixed
	assert(1 == sizeof(hashsize));
	if (253 < hashsize) { LOG(E) << "Hashsize > 253 incorrect token generated"; }
	
	hash.clear();
	// don't hash the devault OPAL password ''
	if (0 == strnlen(password, 32)) {
		goto exit;
	}
	hash.reserve(hashsize + 2); // hope this will prevent reallocation
	for (uint16_t i = 0; i < hashsize; i++) {
		hash.push_back(' ');
	}
	
	cf_pbkdf2_hmac((uint8_t *)password, strnlen(password, 256),
		salt.data(), salt.size(),
		iter,
		hash.data(), hash.size(),
		(DTA_HASH_SHA512 == alg) ? &cf_sha512 : &cf_sha1);

//	gc_pbkdf2_sha1(password, strnlen(password, 256), (const char *)salt.data(), salt.size(), iter,
//		(char *)hash.data(), hash.size());
exit:	// add the token overhead
	hash.insert(hash.begin(), (uint8_t)hash.size());
	hash.insert(hash.begin(), 0xd0);
}

void DtaHashPwd(vector<uint8_t> &hash, char * password, DtaDev * d)
{
    LOG(D1) << " Entered DtaHashPwd";
    char *serNum;

    if (d->no_hash_passwords) {
        hash.clear();
	for (uint16_t i = 0; i < strnlen(password, 32); i++)
		hash.push_back(password[i]);
	// add the token overhead
	hash.insert(hash.begin(), (uint8_t)hash.size());
	hash.insert(hash.begin(), 0xd0);
	LOG(D1) << " Exit DtaHashPwd";
	return;
    }
    serNum = d->getSerialNum();
    vector<uint8_t> salt(serNum, serNum + 20);
    //	vector<uint8_t> salt(DEFAULTSALT);
    DtaHashPassword(hash, password, salt, hashIter, 32, hashAlg);
    LOG(D1) << " Exit DtaHashPwd"; // log for hash timing
}

struct PBKDF_TestTuple
{
    uint8_t hashlen;
    unsigned int iterations;
    const char *Password, *Salt, *hexDerivedKey;
    DtaHashAlg alg;
};

int testresult(std::vector<uint8_t> &result, const char * expected, size_t len) {
	std::vector<uint8_t> work(len);
	printf("Expected Result: %s\nActual Result  : ", expected);
	for (uint32_t i = 0; i < len; i++) { printf("%02x", result[i + 2]); }; printf("\n");
	for (uint32_t i = 0; i < len; i++) {
		unsigned int byte;
		if (1 != sscanf(expected + 2 * i, "%2x", &byte)) return 1;
		work[i] = (uint8_t)byte;
	}
	return memcmp(result.data()+2, work.data(), len);
}

int Testsedutil(const PBKDF_TestTuple *testSet, unsigned int testSetSize)
{
    int pass = 1;
    std::vector<uint8_t> hash, seaSalt;

    for (unsigned int i = 0; i < testSetSize; i++) {
        const PBKDF_TestTuple &tuple = testSet[i];
        hash.clear();
        seaSalt.clear();
        for (uint16_t j = 0; j < strnlen(tuple.Salt, 255); j++) {
            seaSalt.push_back(tuple.Salt[j]);
        }
		printf("%s Password %s Salt %s Iterations %i Length %i\n",
			(DTA_HASH_SHA512 == tuple.alg) ? "SHA512" : "SHA1", (char *)tuple.Password,
			(char *) tuple.Salt, tuple.iterations, tuple.hashlen);
		DtaHashPassword(hash, (char *) tuple.Password, seaSalt, tuple.iterations, tuple.hashlen,
			tuple.alg);
		int fail = (testresult(hash, tuple.hexDerivedKey, tuple.hashlen) == 0);
        pass = pass & fail;
    }

    return pass;
}
int TestPBKDF2()
{
    int pass = 1;
    // from draft-ietf-smime-password-03.txt, at http://www.imc.org/draft-ietf-smime-password
    PBKDF_TestTuple testSet[] = {
        // Draft PKCS #5 PBKDF2 Test Vectors http://tools.ietf.org/html/draft-josefsson-pbkdf2-test-vectors-06
        // "password" (8 octets) S = "salt" (4 octets)	c = 1 DK = 0c60c80f961f0e71f3a9b524af6012062fe037a6
        { 20, 1, "password", "salt", "0c60c80f961f0e71f3a9b524af6012062fe037a6", DTA_HASH_SHA1},
        // "password" (8 octets) S = "salt" (4 octets)	c = 2 DK = ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957
        { 20, 2, "password", "salt", "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957", DTA_HASH_SHA1},
        // "password" (8 octets) S = "salt" (4 octets)	c = 4096 DK = 4b007901b765489abead49d926f721d065a429c1
        { 20, 4096, "password", "salt", "4b007901b765489abead49d926f721d065a429c1", DTA_HASH_SHA1},
        // "password" (8 octets) S = "salt" (4 octets)	c = 16777216 DK = eefe3d61cd4da4e4e9945b3d6ba2158c2634e984
        //{ 20, 16777216, "password", "salt", "eefe3d61cd4da4e4e9945b3d6ba2158c2634e984" },
        // "passwordPASSWORDpassword" (24 octets) S = "saltSALTsaltSALTsaltSALTsaltSALTsalt" (36 octets) c = 4096 DK = 3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038
        { 25, 4096, "passwordPASSWORDpassword", "saltSALTsaltSALTsaltSALTsaltSALTsalt",
            "3d2eec4fe41c849b80c8d83662c0e44a8b291a964cf2f07038", DTA_HASH_SHA1},
        // "pass\0word" (9 octets) S = "sa\0lt" (5 octets)	c = 4096 DK = 56fa6aa75548099dcc37d7f03425e0c3
        //{ 16, 4096, "pass\0word", "sa\0lt", "56fa6aa75548099dcc37d7f03425e0c3" },
        // program receives char * from OS so this test would fail but is not possible IRL
        // PBKDF2-HMAC-SHA512, same inputs
        { 64, 1, "password", "salt",
            "867f70cf1ade02cff3752599a3a53dc4af34c7a669815ae5d513554e1c8cf252c02d470a285a0501bad999bfe943c08f050235d7d68b1da55e63f73b60a57fce",
            DTA_HASH_SHA512},
        { 64, 2, "password", "salt",
            "e1d9c16aa681708a45f5c7c4e215ceb66e011a2e9f0040713f18aefdb866d53cf76cab2868a39b9f7840edce4fef5a82be67335c77a6068e04112754f27ccf4e",
            DTA_HASH_SHA512},
        { 64, 4096, "password", "salt",
            "d197b1b33db0143e018b12f3d1d1479e6cdebdcc97c5c0f87f6902e072f457b5143f30602641b3d55cd335988cb36b84376060ecd532e039b742a239434af2d5",
            DTA_HASH_SHA512},
        { 64, 4096, "passwordPASSWORDpassword", "saltSALTsaltSALTsaltSALTsaltSALTsalt",
            "8c0511f4c6e597c6ac6315d8f0362e225f3c501495ba23b868c005174dc4ee71115b59f9e60cd9532fa33e0f75aefe30225c583a186cd82bd4daea9724a3d3b8",
            DTA_HASH_SHA512},
    };

    cout << "\nPKCS #5 PBKDF2 validation suite running ... \n\n";
    pass = Testsedutil(testSet, sizeof (testSet) / sizeof (testSet[0])) && pass;
    cout << "\nPKCS #5 PBKDF2 validation suite ... ";
    if (pass)
        cout << "passed\n";
    else
        cout << "**FAILED**\n";
    return 0;
}

