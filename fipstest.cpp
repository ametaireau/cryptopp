// fipstest.cpp - written and placed in the public domain by Wei Dai

#include "pch.h"

#ifndef CRYPTOPP_IMPORTS

#define CRYPTOPP_DEFAULT_NO_DLL
#include "dll.h"

#ifdef CRYPTOPP_WIN32_AVAILABLE
#include <windows.h>
#endif

NAMESPACE_BEGIN(CryptoPP)

extern PowerUpSelfTestStatus g_powerUpSelfTestStatus;
SecByteBlock g_actualMac;
unsigned long g_macFileLocation = 0;

const byte * CRYPTOPP_API GetActualMacAndLocation(unsigned int &macSize, unsigned int &fileLocation)
{
	macSize = g_actualMac.size();
	fileLocation = g_macFileLocation;
	return g_actualMac;
}

void KnownAnswerTest(RandomNumberGenerator &rng, const char *output)
{
	EqualityComparisonFilter comparison;

	RandomNumberStore(rng, strlen(output)/2).TransferAllTo(comparison, "0");
	StringSource(output, true, new HexDecoder(new ChannelSwitch(comparison, "1")));

	comparison.ChannelMessageSeriesEnd("0");
	comparison.ChannelMessageSeriesEnd("1");
}

template <class CIPHER>
void X917RNG_KnownAnswerTest(
	const char *key, 
	const char *seed, 
	const char *output,
	unsigned int deterministicTimeVector,
	CIPHER *dummy = NULL)
{
#ifdef OS_RNG_AVAILABLE
	std::string decodedKey, decodedSeed;
	StringSource(key, true, new HexDecoder(new StringSink(decodedKey)));
	StringSource(seed, true, new HexDecoder(new StringSink(decodedSeed)));

	AutoSeededX917RNG<CIPHER> rng;
	rng.Reseed((const byte *)decodedKey.data(), decodedKey.size(), (const byte *)decodedSeed.data(), deterministicTimeVector);
	KnownAnswerTest(rng, output);
#else
	throw 0;
#endif
}

void KnownAnswerTest(StreamTransformation &encryption, StreamTransformation &decryption, const char *plaintext, const char *ciphertext)
{
	EqualityComparisonFilter comparison;

	StringSource(plaintext, true, new HexDecoder(new StreamTransformationFilter(encryption, new ChannelSwitch(comparison, "0"), StreamTransformationFilter::NO_PADDING)));
	StringSource(ciphertext, true, new HexDecoder(new ChannelSwitch(comparison, "1")));

	StringSource(ciphertext, true, new HexDecoder(new StreamTransformationFilter(decryption, new ChannelSwitch(comparison, "0"), StreamTransformationFilter::NO_PADDING)));
	StringSource(plaintext, true, new HexDecoder(new ChannelSwitch(comparison, "1")));

	comparison.ChannelMessageSeriesEnd("0");
	comparison.ChannelMessageSeriesEnd("1");
}

template <class CIPHER>
void SymmetricEncryptionKnownAnswerTest(
	const char *key, 
	const char *hexIV, 
	const char *plaintext, 
	const char *ecb,
	const char *cbc,
	const char *cfb,
	const char *ofb,
	const char *ctr,
	CIPHER *dummy = NULL)
{
	std::string decodedKey;
	StringSource(key, true, new HexDecoder(new StringSink(decodedKey)));

	typename CIPHER::Encryption encryption((const byte *)decodedKey.data(), decodedKey.size());
	typename CIPHER::Decryption decryption((const byte *)decodedKey.data(), decodedKey.size());

	SecByteBlock iv(encryption.BlockSize());
	StringSource(hexIV, true, new HexDecoder(new ArraySink(iv, iv.size())));

	if (ecb)
		KnownAnswerTest(ECB_Mode_ExternalCipher::Encryption(encryption).Ref(), ECB_Mode_ExternalCipher::Decryption(decryption).Ref(), plaintext, ecb);
	if (cbc)
		KnownAnswerTest(CBC_Mode_ExternalCipher::Encryption(encryption, iv).Ref(), CBC_Mode_ExternalCipher::Decryption(decryption, iv).Ref(), plaintext, cbc);
	if (cfb)
		KnownAnswerTest(CFB_Mode_ExternalCipher::Encryption(encryption, iv).Ref(), CFB_Mode_ExternalCipher::Decryption(encryption, iv).Ref(), plaintext, cfb);
	if (ofb)
		KnownAnswerTest(OFB_Mode_ExternalCipher::Encryption(encryption, iv).Ref(), OFB_Mode_ExternalCipher::Decryption(encryption, iv).Ref(), plaintext, ofb);
	if (ctr)
		KnownAnswerTest(CTR_Mode_ExternalCipher::Encryption(encryption, iv).Ref(), CTR_Mode_ExternalCipher::Decryption(encryption, iv).Ref(), plaintext, ctr);
}

void KnownAnswerTest(HashTransformation &hash, const char *message, const char *digest)
{
	EqualityComparisonFilter comparison;
	StringSource(digest, true, new HexDecoder(new ChannelSwitch(comparison, "1")));
	StringSource(message, true, new HashFilter(hash, new ChannelSwitch(comparison, "0")));

	comparison.ChannelMessageSeriesEnd("0");
	comparison.ChannelMessageSeriesEnd("1");
}

template <class HASH>
void SecureHashKnownAnswerTest(const char *message, const char *digest, HASH *dummy = NULL)
{
	HASH hash;
	KnownAnswerTest(hash, message, digest);
}

template <class MAC>
void MAC_KnownAnswerTest(const char *key, const char *message, const char *digest, MAC *dummy = NULL)
{
	std::string decodedKey;
	StringSource(key, true, new HexDecoder(new StringSink(decodedKey)));

	MAC mac((const byte *)decodedKey.data(), decodedKey.size());
	KnownAnswerTest(mac, message, digest);
}

template <class SCHEME>
void SignatureKnownAnswerTest(const char *key, const char *message, const char *signature, SCHEME *dummy = NULL)
{
#ifdef OS_RNG_AVAILABLE
	AutoSeededX917RNG<DES_EDE3> rng;
#else
	RandomNumberGenerator &rng = NullRNG();
#endif

	typename SCHEME::Signer signer(StringSource(key, true, new HexDecoder).Ref());
	typename SCHEME::Verifier verifier(signer);

	EqualityComparisonFilter comparison;

	StringSource(message, true, new SignerFilter(rng, signer, new ChannelSwitch(comparison, "0")));
	StringSource(signature, true, new HexDecoder(new ChannelSwitch(comparison, "1")));

	comparison.ChannelMessageSeriesEnd("0");
	comparison.ChannelMessageSeriesEnd("1");

	VerifierFilter verifierFilter(verifier, NULL, VerifierFilter::SIGNATURE_AT_BEGIN | VerifierFilter::THROW_EXCEPTION);
	StringSource(signature, true, new HexDecoder(new Redirector(verifierFilter, Redirector::DATA_ONLY)));
	StringSource(message, true, new Redirector(verifierFilter));
}

void EncryptionPairwiseConsistencyTest(const PK_Encryptor &encryptor, const PK_Decryptor &decryptor)
{
	try
	{
#ifdef OS_RNG_AVAILABLE
		AutoSeededX917RNG<DES_EDE3> rng;
#else
		RandomNumberGenerator &rng = NullRNG();
#endif
		const char *testMessage ="test message";

		EqualityComparisonFilter comparison;
		comparison.ChannelPutMessageEnd("0", (const byte *)testMessage, strlen(testMessage));

		StringSource(
			testMessage, 
			true, 
			new PK_EncryptorFilter(
				rng, 
				encryptor, 
				new PK_DecryptorFilter(rng, decryptor, new ChannelSwitch(comparison, "1"))));

		comparison.ChannelMessageSeriesEnd("0");
		comparison.ChannelMessageSeriesEnd("1");
	}
	catch (...)
	{
		throw SelfTestFailure(encryptor.AlgorithmName() + ": pairwise consistency test failed");
	}
}

void SignaturePairwiseConsistencyTest(const PK_Signer &signer, const PK_Verifier &verifier)
{
	try
	{
#ifdef OS_RNG_AVAILABLE
		AutoSeededX917RNG<DES_EDE3> rng;
#else
		RandomNumberGenerator &rng = NullRNG();
#endif

		StringSource(
			"test message", 
			true, 
			new SignerFilter(
				rng, 
				signer, 
				new VerifierFilter(verifier, NULL, VerifierFilter::THROW_EXCEPTION),
				true));
	}
	catch (...)
	{
		throw SelfTestFailure(signer.AlgorithmName() + ": pairwise consistency test failed");
	}
}

template <class SCHEME>
void SignaturePairwiseConsistencyTest(const char *key, SCHEME *dummy = NULL)
{
	typename SCHEME::Signer signer(StringSource(key, true, new HexDecoder).Ref());
	typename SCHEME::Verifier verifier(signer);

	SignaturePairwiseConsistencyTest(signer, verifier);
}

MessageAuthenticationCode * NewIntegrityCheckingMAC()
{
	byte key[] = {0x47, 0x1E, 0x33, 0x96, 0x65, 0xB1, 0x6A, 0xED, 0x0B, 0xF8, 0x6B, 0xFD, 0x01, 0x65, 0x05, 0xCC};
	return new HMAC<SHA1>(key, sizeof(key));
}

bool IntegrityCheckModule(const char *moduleFilename, const byte *expectedModuleMac, SecByteBlock *pActualMac, unsigned long *pMacFileLocation)
{
	std::auto_ptr<MessageAuthenticationCode> mac(NewIntegrityCheckingMAC());
	unsigned int macSize = mac->DigestSize();

	SecByteBlock tempMac;
	SecByteBlock &actualMac = pActualMac ? *pActualMac : tempMac;
	actualMac.resize(macSize);

	unsigned long tempLocation;
	unsigned long &macFileLocation = pMacFileLocation ? *pMacFileLocation : tempLocation;
	macFileLocation = 0;

	HashFilter verifier(*mac, new ArraySink(actualMac, actualMac.size()));
//	FileSink verifier("c:\\dt.tmp");
	FileStore file(moduleFilename);

#ifdef CRYPTOPP_WIN32_AVAILABLE
	// try to hash from memory first
	HMODULE h = GetModuleHandle(moduleFilename);
	const byte *memBase = (const byte *)h;
	IMAGE_DOS_HEADER *ph = (IMAGE_DOS_HEADER *)h;
	IMAGE_NT_HEADERS *phnt = (IMAGE_NT_HEADERS *)((byte *)h + ph->e_lfanew);
	IMAGE_SECTION_HEADER *phs = IMAGE_FIRST_SECTION(phnt);
	DWORD nSections = phnt->FileHeader.NumberOfSections;
	DWORD currentFilePos = 0;

	while (nSections--)
	{
		switch (phs->Characteristics)
		{
		default:
			break;
		case IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ:
		case IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ:
			unsigned int sectionSize = STDMIN(phs->SizeOfRawData, phs->Misc.VirtualSize);
			const byte *sectionMemStart = memBase + phs->VirtualAddress;
			unsigned int sectionFileStart = phs->PointerToRawData;
			unsigned int subSectionStart = 0, nextSubSectionStart;

			do
			{
				const byte *subSectionMemStart = sectionMemStart + subSectionStart;
				unsigned int subSectionFileStart = sectionFileStart + subSectionStart;
				unsigned int subSectionSize = sectionSize - subSectionStart;
				nextSubSectionStart = 0;

				unsigned int entriesToReadFromDisk[] = {IMAGE_DIRECTORY_ENTRY_IMPORT, IMAGE_DIRECTORY_ENTRY_IAT};
				for (unsigned int i=0; i<sizeof(entriesToReadFromDisk)/sizeof(entriesToReadFromDisk[0]); i++)
				{
					const IMAGE_DATA_DIRECTORY &entry = phnt->OptionalHeader.DataDirectory[entriesToReadFromDisk[i]];
					const byte *entryMemStart = memBase + entry.VirtualAddress;
					if (subSectionMemStart <= entryMemStart && entryMemStart < subSectionMemStart + subSectionSize)
					{
						subSectionSize = entryMemStart - subSectionMemStart;
						nextSubSectionStart = entryMemStart - sectionMemStart + entry.Size;
					}
				}

				file.TransferTo(verifier, subSectionFileStart - currentFilePos);
				if (subSectionMemStart <= expectedModuleMac && expectedModuleMac < subSectionMemStart + subSectionSize)
				{
					// skip over the MAC
					verifier.Put(subSectionMemStart, expectedModuleMac - subSectionMemStart);
					verifier.Put(expectedModuleMac + macSize, subSectionSize - macSize - (expectedModuleMac - subSectionMemStart));
					macFileLocation = subSectionFileStart + (expectedModuleMac - subSectionMemStart);
				}
				else
					verifier.Put(subSectionMemStart, subSectionSize);
				file.Skip(subSectionSize);
				currentFilePos = subSectionFileStart + subSectionSize;
				subSectionStart = nextSubSectionStart;
			} while (nextSubSectionStart != 0);
		}
		phs++;
	}
#endif
	file.TransferAllTo(verifier);

#ifdef CRYPTOPP_WIN32_AVAILABLE
	// if that fails (could be caused by debug breakpoints or DLL base relocation modifying image in memory),
	// hash from disk instead
	if (memcmp(expectedModuleMac, actualMac, macSize) != 0)
	{
		OutputDebugString("In memory integrity check failed. This may be caused by debug breakpoints or DLL relocation.\n");
		file.Initialize(MakeParameters("InputFileName", moduleFilename));
		verifier.Detach(new ArraySink(actualMac, actualMac.size()));
		if (macFileLocation)
		{
			file.TransferTo(verifier, macFileLocation);
			file.Skip(macSize);
		}
		file.TransferAllTo(verifier);
	}
#endif

	if (memcmp(expectedModuleMac, actualMac, macSize) == 0)
		return true;

#ifdef CRYPTOPP_WIN32_AVAILABLE
	std::string hexMac;
	HexEncoder(new StringSink(hexMac)).PutMessageEnd(actualMac, actualMac.size());
	OutputDebugString((moduleFilename + (" integrity check failed. Actual MAC is: " + hexMac) + "\n").c_str());
#endif
	return false;
}

void DoPowerUpSelfTest(const char *moduleFilename, const byte *expectedModuleMac)
{
	g_powerUpSelfTestStatus = POWER_UP_SELF_TEST_NOT_DONE;
	SetPowerUpSelfTestInProgressOnThisThread(true);

	try
	{
		if (FIPS_140_2_ComplianceEnabled() || moduleFilename != NULL)
		{
			if (!IntegrityCheckModule(moduleFilename, expectedModuleMac, &g_actualMac, &g_macFileLocation))
				throw 0;	// throw here so we break in the debugger, this will be caught right away
		}

		// algorithm tests

		X917RNG_KnownAnswerTest<DES_EDE3>(
			"48851090B4992453E83CDA86416534E53EA2FCE1A0B3A40C",						// key
			"7D00BD0A79F6B0F5",														// seed
			"22B590B08B53363AEB89AD65F81A5B6FB83F326CE06BF35751E6C41B43B729C4",		// output
			1489728269);															// time vector

/*		SymmetricEncryptionKnownAnswerTest<DES>(
			"0123456789abcdef",	// key
			"1234567890abcdef",	// IV
			"4e6f77206973207468652074696d6520666f7220616c6c20",	// plaintext
			"3fa40e8a984d48156a271787ab8883f9893d51ec4b563b53",	// ecb
			"E5C7CDDE872BF27C43E934008C389C0F683788499A7C05F6",	// cbc
			"F3096249C7F46E51A69E839B1A92F78403467133898EA622", // cfb
			"f3096249c7f46e5135f24a242eeb3d3f3d6d5be3255af8c3", // ofb
			"F3096249C7F46E51163A8CA0FFC94C27FA2F80F480B86F75");// ctr
*/
		SymmetricEncryptionKnownAnswerTest<DES_EDE3>(
			"385D7189A5C3D485E1370AA5D408082B5CCCCB5E19F2D90E",
			"C141B5FCCD28DC8A",
			"6E1BD7C6120947A464A6AAB293A0F89A563D8D40D3461B68",
			"64EAAD4ACBB9CEAD6C7615E7C7E4792FE587D91F20C7D2F4",
			"6235A461AFD312973E3B4F7AA7D23E34E03371F8E8C376C9",
			"E26BA806A59B0330DE40CA38E77A3E494BE2B212F6DD624B",
			"E26BA806A59B03307DE2BCC25A08BA40A8BA335F5D604C62",
			"E26BA806A59B03303C62C2EFF32D3ACDD5D5F35EBCC53371");

		SymmetricEncryptionKnownAnswerTest<SKIPJACK>(
			"1555E5531C3A169B2D65",
			"6EC9795701F49864",
			"00AFA48E9621E52E8CBDA312660184EDDB1F33D9DACDA8DA",
			"DBEC73562EFCAEB56204EB8AE9557EBF77473FBB52D17CD1",
			"0C7B0B74E21F99B8F2C8DF37879F6C044967F42A796DCA8B",
			"79FDDA9724E36CC2E023E9A5C717A8A8A7FDA465CADCBF63",
			"79FDDA9724E36CC26CACBD83C1ABC06EAF5B249BE5B1E040",
			"79FDDA9724E36CC211B0AEC607B95A96BCDA318440B82F49");

		SymmetricEncryptionKnownAnswerTest<AES>(
			"2b7e151628aed2a6abf7158809cf4f3c",
			"000102030405060708090a0b0c0d0e0f",
			"6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710",	// plaintext
			"3ad77bb40d7a3660a89ecaf32466ef97f5d3d58503b9699de785895a96fdbaaf43b1cd7f598ece23881b00e3ed0306887b0c785e27e8ad3f8223207104725dd4", // ecb
			"7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b273bed6b8e3c1743b7116e69e222295163ff1caa1681fac09120eca307586e1a7",	// cbc
			"3b3fd92eb72dad20333449f8e83cfb4ac8a64537a0b3a93fcde3cdad9f1ce58b26751f67a3cbb140b1808cf187a4f4dfc04b05357c5d1c0eeac4c66f9ff7f2e6", // cfb
			"3b3fd92eb72dad20333449f8e83cfb4a7789508d16918f03f53c52dac54ed8259740051e9c5fecf64344f7a82260edcc304c6528f659c77866a510d9c1d6ae5e", // ofb
			NULL);

		SymmetricEncryptionKnownAnswerTest<AES>(
			"2b7e151628aed2a6abf7158809cf4f3c",
			"f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff",
			"6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e5130c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710",
			NULL,
			NULL,
			NULL,
			NULL,
			"874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff5ae4df3edbd5d35e5b4f09020db03eab1e031dda2fbe03d1792170a0f3009cee"); // ctr


		SecureHashKnownAnswerTest<SHA>(
			"abc",
			"A9993E364706816ABA3E25717850C26C9CD0D89D");
/*
		SecureHashKnownAnswerTest<SHA256>(
			"abc",
			"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

		SecureHashKnownAnswerTest<SHA384>(
			"abc",
			"cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7");

		SecureHashKnownAnswerTest<SHA512>(
			"abc",
			"ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
*/
		MAC_KnownAnswerTest<HMAC<SHA> >(
			"303132333435363738393a3b3c3d3e3f40414243",
			"Sample #2",
			"0922d3405faa3d194f82a45830737d5cc6c75d24");
/*
		MAC_KnownAnswerTest<HMAC<SHA256> >(
			"303132333435363738393a3b3c3d3e3f40414243",
			"abc",
			"D28363F335B2DAE468793A38680DEA9F7FB8BE1DCEDA197CDB3B1CB59A9F6422");

		MAC_KnownAnswerTest<HMAC<SHA384> >(
			"303132333435363738393a3b3c3d3e3f40414243",
			"abc",
			"E7740C592F1414C969190EFACF51FC8BE1CB52F5DC5E686200D2CA1773D151DB19C59112371CE374165A6BF72AEF69D0");

		MAC_KnownAnswerTest<HMAC<SHA512> >(
			"303132333435363738393a3b3c3d3e3f40414243",
			"abc",
			"BF07864E733B995862F3C2D432C7FF2F5EB073FFFC4F880CD94D5D21086476B7428F27BE694A9D9CB3BB500FE1255852BAFCBAF4042390B3706CDF02421B51AC");
*/
		SignatureKnownAnswerTest<RSASS<PKCS1v15, SHA> >(
			"30820150020100300d06092a864886f70d01010105000482013a3082013602010002400a66791dc6988168de7ab77419bb7fb0"
			"c001c62710270075142942e19a8d8c51d053b3e3782a1de5dc5af4ebe99468170114a1dfe67cdc9a9af55d655620bbab0203010001"
			"02400123c5b61ba36edb1d3679904199a89ea80c09b9122e1400c09adcf7784676d01d23356a7d44d6bd8bd50e94bfc723fa"
			"87d8862b75177691c11d757692df8881022033d48445c859e52340de704bcdda065fbb4058d740bd1d67d29e9c146c11cf61"
			"0220335e8408866b0fd38dc7002d3f972c67389a65d5d8306566d5c4f2a5aa52628b0220045ec90071525325d3d46db79695e9af"
			"acc4523964360e02b119baa366316241022015eb327360c7b60d12e5e2d16bdcd97981d17fba6b70db13b20b436e24eada590220"
			"2ca6366d72781dfa24d34a9a24cbc2ae927a9958af426563ff63fb11658a461d",
			"Everyone gets Friday off.",
			"0610761F95FFD1B8F29DA34212947EC2AA0E358866A722F03CC3C41487ADC604A48FF54F5C6BEDB9FB7BD59F82D6E55D8F3174BA361B2214B2D74E8825E04E81");

		SignaturePairwiseConsistencyTest<DSA>(
			"3082014A0201003082012B06072A8648CE3804013082011E02818100F468699A6F6EBCC0120D3B34C8E007F125EC7D81F763B8D0F33869AE3BD6B9F2ECCC7DF34DF84C0307449E9B85D30D57194BCCEB310F48141914DD13A077AAF9B624A6CBE666BBA1D7EBEA95B5BA6F54417FD5D4E4220C601E071D316A24EA814E8B0122DBF47EE8AEEFD319EBB01DD95683F10DBB4FEB023F8262A07EAEB7FD02150082AD4E034DA6EEACDFDAE68C36F2BAD614F9E53B02818071AAF73361A26081529F7D84078ADAFCA48E031DB54AD57FB1A833ADBD8672328AABAA0C756247998D7A5B10DACA359D231332CE8120B483A784FE07D46EEBFF0D7D374A10691F78653E6DC29E27CCB1B174923960DFE5B959B919B2C3816C19251832AFD8E35D810E598F82877ABF7D40A041565168BD7F0E21E3FE2A8D8C1C0416021426EBA66E846E755169F84A1DA981D86502405DDF");

		SignaturePairwiseConsistencyTest<ECDSA<EC2N, SHA> >(
			"302D020100301006072A8648CE3D020106052B8104000404163014020101040F0070337065E1E196980A9D00E37211");

		SignaturePairwiseConsistencyTest<ECDSA<ECP, SHA> >(
			"3039020100301306072A8648CE3D020106082A8648CE3D030101041F301D02010104182BB8A13C8B867010BD9471D9E81FDB01ABD0538C64D6249A");
	}
	catch (...)
	{
		g_powerUpSelfTestStatus = POWER_UP_SELF_TEST_FAILED;
		goto done;
	}

	g_powerUpSelfTestStatus = POWER_UP_SELF_TEST_PASSED;

done:
	SetPowerUpSelfTestInProgressOnThisThread(false);
	return;
}

#ifdef CRYPTOPP_WIN32_AVAILABLE

static const byte s_moduleMac[CryptoPP::HMAC<CryptoPP::SHA1>::DIGESTSIZE] = "reserved for mac";
static HMODULE s_hModule = NULL;

void DoDllPowerUpSelfTest()
{
	char moduleFileName[MAX_PATH];
	GetModuleFileNameA(s_hModule, moduleFileName, sizeof(moduleFileName));
	CryptoPP::DoPowerUpSelfTest(moduleFileName, s_moduleMac);
}

#else

void DoDllPowerUpSelfTest()
{
	throw NotImplemented("DoDllPowerUpSelfTest() only available on Windows");
}

#endif	// #ifdef CRYPTOPP_WIN32_AVAILABLE

NAMESPACE_END

#ifdef CRYPTOPP_WIN32_AVAILABLE

// DllMain needs to be in the global namespace
BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  ul_reason_for_call, 
                      LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		CryptoPP::s_hModule = (HMODULE)hModule;
		CryptoPP::DoDllPowerUpSelfTest();
	}
    return TRUE;
}

#endif	// #ifdef CRYPTOPP_WIN32_AVAILABLE

#endif	// #ifndef CRYPTOPP_IMPORTS
