/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*****************************************************************************/
/* This is an automatically generated file. If you're not                    */
/* PublicKeyPinningService.cpp, you shouldn't be #including it.              */
/*****************************************************************************/
#include <stdint.h>
/* AddTrust External Root */
static const char kAddTrust_External_RootFingerprint[] =
  "lCppFqbkrlJ3EcVFAkeip0+44VaoJUymbnOaEUk7tEU=";

/* AddTrust Low-Value Services Root */
static const char kAddTrust_Low_Value_Services_RootFingerprint[] =
  "BStocQfshOhzA4JFLsKidFF0XXSFpX1vRk4Np6G2ryo=";

/* AddTrust Public Services Root */
static const char kAddTrust_Public_Services_RootFingerprint[] =
  "OGHXtpYfzbISBFb/b8LrdwSxp0G0vZM6g3b14ZFcppg=";

/* AddTrust Qualified Certificates Root */
static const char kAddTrust_Qualified_Certificates_RootFingerprint[] =
  "xzr8Lrp3DQy8HuQfJStS6Kk9ErctzOwDHY2DnL+Bink=";

/* AffirmTrust Commercial */
static const char kAffirmTrust_CommercialFingerprint[] =
  "bEZLmlsjOl6HTadlwm8EUBDS3c/0V5TwtMfkqvpQFJU=";

/* AffirmTrust Networking */
static const char kAffirmTrust_NetworkingFingerprint[] =
  "lAcq0/WPcPkwmOWl9sBMlscQvYSdgxhJGa6Q64kK5AA=";

/* AffirmTrust Premium */
static const char kAffirmTrust_PremiumFingerprint[] =
  "x/Q7TPW3FWgpT4IrU3YmBfbd0Vyt7Oc56eLDy6YenWc=";

/* AffirmTrust Premium ECC */
static const char kAffirmTrust_Premium_ECCFingerprint[] =
  "MhmwkRT/SVo+tusAwu/qs0ACrl8KVsdnnqCHo/oDfk8=";

/* Baltimore CyberTrust Root */
static const char kBaltimore_CyberTrust_RootFingerprint[] =
  "Y9mvm0exBk1JoQ57f9Vm28jKo5lFm/woKcVxrYxu80o=";

/* COMODO Certification Authority */
static const char kCOMODO_Certification_AuthorityFingerprint[] =
  "AG1751Vd2CAmRCxPGieoDomhmJy4ezREjtIZTBgZbV4=";

/* COMODO ECC Certification Authority */
static const char kCOMODO_ECC_Certification_AuthorityFingerprint[] =
  "58qRu/uxh4gFezqAcERupSkRYBlBAvfcw7mEjGPLnNU=";

/* COMODO RSA Certification Authority */
static const char kCOMODO_RSA_Certification_AuthorityFingerprint[] =
  "grX4Ta9HpZx6tSHkmCrvpApTQGo67CYDnvprLg5yRME=";

/* Comodo AAA Services root */
static const char kComodo_AAA_Services_rootFingerprint[] =
  "vRU+17BDT2iGsXvOi76E7TQMcTLXAqj0+jGPdW7L1vM=";

/* Comodo Secure Services root */
static const char kComodo_Secure_Services_rootFingerprint[] =
  "RpHL/ehKa2BS3b4VK7DCFq4lqG5XR4E9vA8UfzOFcL4=";

/* Comodo Trusted Services root */
static const char kComodo_Trusted_Services_rootFingerprint[] =
  "4tiR77c4ZpEF1TDeXtcuKyrD9KZweLU0mz/ayklvXrg=";

/* Cybertrust Global Root */
static const char kCybertrust_Global_RootFingerprint[] =
  "foeCwVDOOVL4AuY2AjpdPpW7XWjjPoWtsroXgSXOvxU=";

/* DST Root CA X3 */
static const char kDST_Root_CA_X3Fingerprint[] =
  "Vjs8r4z+80wjNcr1YKepWQboSIRi63WsWXhIMN+eWys=";

/* DigiCert Assured ID Root CA */
static const char kDigiCert_Assured_ID_Root_CAFingerprint[] =
  "I/Lt/z7ekCWanjD0Cvj5EqXls2lOaThEA0H2Bg4BT/o=";

/* DigiCert Assured ID Root G2 */
static const char kDigiCert_Assured_ID_Root_G2Fingerprint[] =
  "8ca6Zwz8iOTfUpc8rkIPCgid1HQUT+WAbEIAZOFZEik=";

/* DigiCert Assured ID Root G3 */
static const char kDigiCert_Assured_ID_Root_G3Fingerprint[] =
  "Fe7TOVlLME+M+Ee0dzcdjW/sYfTbKwGvWJ58U7Ncrkw=";

/* DigiCert Global Root CA */
static const char kDigiCert_Global_Root_CAFingerprint[] =
  "r/mIkG3eEpVdm+u/ko/cwxzOMo1bk4TyHIlByibiA5E=";

/* DigiCert Global Root G2 */
static const char kDigiCert_Global_Root_G2Fingerprint[] =
  "i7WTqTvh0OioIruIfFR4kMPnBqrS2rdiVPl/s2uC/CY=";

/* DigiCert Global Root G3 */
static const char kDigiCert_Global_Root_G3Fingerprint[] =
  "uUwZgwDOxcBXrQcntwu+kYFpkiVkOaezL0WYEZ3anJc=";

/* DigiCert High Assurance EV Root CA */
static const char kDigiCert_High_Assurance_EV_Root_CAFingerprint[] =
  "WoiWRyIOVNa9ihaBciRSC7XHjliYS9VwUGOIud4PB18=";

/* DigiCert Trusted Root G4 */
static const char kDigiCert_Trusted_Root_G4Fingerprint[] =
  "Wd8xe/qfTwq3ylFNd3IpaqLHZbh2ZNCLluVzmeNkcpw=";

/* End Entity Test Cert */
static const char kEnd_Entity_Test_CertFingerprint[] =
  "VCIlmPM9NkgFQtrs4Oa5TeFcDu6MWRTKSNdePEhOgD8=";

/* Entrust Root Certification Authority */
static const char kEntrust_Root_Certification_AuthorityFingerprint[] =
  "bb+uANN7nNc/j7R95lkXrwDg3d9C286sIMF8AnXuIJU=";

/* Entrust Root Certification Authority - EC1 */
static const char kEntrust_Root_Certification_Authority___EC1Fingerprint[] =
  "/qK31kX7pz11PB7Jp4cMQOH3sMVh6Se5hb9xGGbjbyI=";

/* Entrust Root Certification Authority - G2 */
static const char kEntrust_Root_Certification_Authority___G2Fingerprint[] =
  "du6FkDdMcVQ3u8prumAo6t3i3G27uMP2EOhR8R0at/U=";

/* Entrust.net Premium 2048 Secure Server CA */
static const char kEntrust_net_Premium_2048_Secure_Server_CAFingerprint[] =
  "HqPF5D7WbC2imDpCpKebHpBnhs6fG1hiFBmgBGOofTg=";

/* FacebookBackup */
static const char kFacebookBackupFingerprint[] =
  "q4PO2G2cbkZhZ82+JgmRUyGMoAeozA+BSXVXQWB8XWQ=";

/* GOOGLE_PIN_COMODORSADomainValidationSecureServerCA */
static const char kGOOGLE_PIN_COMODORSADomainValidationSecureServerCAFingerprint[] =
  "klO23nT2ehFDXCfx3eHTDRESMz3asj1muO+4aIdjiuY=";

/* GOOGLE_PIN_DigiCertECCSecureServerCA */
static const char kGOOGLE_PIN_DigiCertECCSecureServerCAFingerprint[] =
  "PZXN3lRAy+8tBKk2Ox6F7jIlnzr2Yzmwqc3JnyfXoCw=";

/* GOOGLE_PIN_DigiCertSHA2HighAssuranceServerCA */
static const char kGOOGLE_PIN_DigiCertSHA2HighAssuranceServerCAFingerprint[] =
  "k2v657xBsOVe1PQRwOsHsw3bsGT2VzIqz5K+59sNQws=";

/* GOOGLE_PIN_Entrust_SSL */
static const char kGOOGLE_PIN_Entrust_SSLFingerprint[] =
  "nsxRNo6G40YPZsKV5JQt1TCA8nseQQr/LRqp1Oa8fnw=";

/* GOOGLE_PIN_GTECyberTrustGlobalRoot */
static const char kGOOGLE_PIN_GTECyberTrustGlobalRootFingerprint[] =
  "EGn6R6CqT4z3ERscrqNl7q7RC//zJmDe9uBhS/rnCHU=";

/* GOOGLE_PIN_GoDaddySecure */
static const char kGOOGLE_PIN_GoDaddySecureFingerprint[] =
  "MrZLZnJ6IGPkBm87lYywqu5Xal7O/ZUzmbuIdHMdlYc=";

/* GOOGLE_PIN_GoogleG2 */
static const char kGOOGLE_PIN_GoogleG2Fingerprint[] =
  "7HIpactkIAq2Y49orFOOQKurWxmmSFZhBCoQYcRhJ3Y=";

/* GOOGLE_PIN_LetsEncryptAuthorityBackup_X2_X4 */
static const char kGOOGLE_PIN_LetsEncryptAuthorityBackup_X2_X4Fingerprint[] =
  "sRHdihwgkaib1P1gxX8HFszlD+7/gTfNvuAybgLPNis=";

/* GOOGLE_PIN_LetsEncryptAuthorityPrimary_X1_X3 */
static const char kGOOGLE_PIN_LetsEncryptAuthorityPrimary_X1_X3Fingerprint[] =
  "YLh1dUR9y6Kja30RrAn7JKnbQG/uEtLMkBgFF2Fuihg=";

/* GOOGLE_PIN_RapidSSL */
static const char kGOOGLE_PIN_RapidSSLFingerprint[] =
  "lT09gPUeQfbYrlxRtpsHrjDblj9Rpz+u7ajfCrg4qDM=";

/* GOOGLE_PIN_SymantecClass3EVG3 */
static const char kGOOGLE_PIN_SymantecClass3EVG3Fingerprint[] =
  "gMxWOrX4PMQesK9qFNbYBxjBfjUvlkn/vN1n+L9lE5E=";

/* GOOGLE_PIN_UTNDATACorpSGC */
static const char kGOOGLE_PIN_UTNDATACorpSGCFingerprint[] =
  "QAL80xHQczFWfnG82XHkYEjI3OjRZZcRdTs9qiommvo=";

/* GOOGLE_PIN_VeriSignClass1 */
static const char kGOOGLE_PIN_VeriSignClass1Fingerprint[] =
  "LclHC+Y+9KzxvYKGCUArt7h72ZY4pkOTTohoLRvowwg=";

/* GOOGLE_PIN_VeriSignClass2_G2 */
static const char kGOOGLE_PIN_VeriSignClass2_G2Fingerprint[] =
  "2oALgLKofTmeZvoZ1y/fSZg7R9jPMix8eVA6DH4o/q8=";

/* GOOGLE_PIN_VeriSignClass3_G2 */
static const char kGOOGLE_PIN_VeriSignClass3_G2Fingerprint[] =
  "AjyBzOjnxk+pQtPBUEhwfTXZu1uH9PVExb8bxWQ68vo=";

/* GOOGLE_PIN_VeriSignClass4_G3 */
static const char kGOOGLE_PIN_VeriSignClass4_G3Fingerprint[] =
  "VnuCEf0g09KD7gzXzgZyy52ZvFtIeljJ1U7Gf3fUqPU=";

/* GeoTrust Global CA */
static const char kGeoTrust_Global_CAFingerprint[] =
  "h6801m+z8v3zbgkRHpq6L29Esgfzhj89C1SyUCOQmqU=";

/* GeoTrust Global CA 2 */
static const char kGeoTrust_Global_CA_2Fingerprint[] =
  "F3VaXClfPS1y5vAxofB/QAxYi55YKyLxfq4xoVkNEYU=";

/* GeoTrust Primary Certification Authority */
static const char kGeoTrust_Primary_Certification_AuthorityFingerprint[] =
  "SQVGZiOrQXi+kqxcvWWE96HhfydlLVqFr4lQTqI5qqo=";

/* GeoTrust Primary Certification Authority - G2 */
static const char kGeoTrust_Primary_Certification_Authority___G2Fingerprint[] =
  "vPtEqrmtAhAVcGtBIep2HIHJ6IlnWQ9vlK50TciLePs=";

/* GeoTrust Primary Certification Authority - G3 */
static const char kGeoTrust_Primary_Certification_Authority___G3Fingerprint[] =
  "q5hJUnat8eyv8o81xTBIeB5cFxjaucjmelBPT2pRMo8=";

/* GeoTrust Universal CA */
static const char kGeoTrust_Universal_CAFingerprint[] =
  "lpkiXF3lLlbN0y3y6W0c/qWqPKC7Us2JM8I7XCdEOCA=";

/* GeoTrust Universal CA 2 */
static const char kGeoTrust_Universal_CA_2Fingerprint[] =
  "fKoDRlEkWQxgHlZ+UhSOlSwM/+iQAFMP4NlbbVDqrkE=";

/* GlobalSign ECC Root CA - R4 */
static const char kGlobalSign_ECC_Root_CA___R4Fingerprint[] =
  "CLOmM1/OXvSPjw5UOYbAf9GKOxImEp9hhku9W90fHMk=";

/* GlobalSign ECC Root CA - R5 */
static const char kGlobalSign_ECC_Root_CA___R5Fingerprint[] =
  "fg6tdrtoGdwvVFEahDVPboswe53YIFjqbABPAdndpd8=";

/* GlobalSign Root CA */
static const char kGlobalSign_Root_CAFingerprint[] =
  "K87oWBWM9UZfyddvDfoxL+8lpNyoUB2ptGtn0fv6G2Q=";

/* GlobalSign Root CA - R2 */
static const char kGlobalSign_Root_CA___R2Fingerprint[] =
  "iie1VXtL7HzAMF+/PVPR9xzT80kQxdZeJ+zduCB3uj0=";

/* GlobalSign Root CA - R3 */
static const char kGlobalSign_Root_CA___R3Fingerprint[] =
  "cGuxAXyFXFkWm61cF4HPWX8S0srS9j0aSqN0k4AP+4A=";

/* Go Daddy Class 2 CA */
static const char kGo_Daddy_Class_2_CAFingerprint[] =
  "VjLZe/p3W/PJnd6lL8JVNBCGQBZynFLdZSTIqcO0SJ8=";

/* Go Daddy Root Certificate Authority - G2 */
static const char kGo_Daddy_Root_Certificate_Authority___G2Fingerprint[] =
  "Ko8tivDrEjiY90yGasP6ZpBU4jwXvHqVvQI0GS3GNdA=";

/* GoogleBackup2048 */
static const char kGoogleBackup2048Fingerprint[] =
  "IPMbDAjLVSGntGO3WP53X/zilCVndez5YJ2+vJvhJsA=";

/* SpiderOak2 */
static const char kSpiderOak2Fingerprint[] =
  "7Y3UnxbffL8aFPXsOJBpGasgpDmngpIhAxGKdQRklQQ=";

/* SpiderOak3 */
static const char kSpiderOak3Fingerprint[] =
  "LkER54vOdlygpTsbYvlpMq1CE/lDAG1AP9xmdtwvV2A=";

/* Starfield Class 2 CA */
static const char kStarfield_Class_2_CAFingerprint[] =
  "FfFKxFycfaIz00eRZOgTf+Ne4POK6FgYPwhBDqgqxLQ=";

/* Starfield Root Certificate Authority - G2 */
static const char kStarfield_Root_Certificate_Authority___G2Fingerprint[] =
  "gI1os/q0iEpflxrOfRBVDXqVoWN3Tz7Dav/7IT++THQ=";

/* Swehack */
static const char kSwehackFingerprint[] =
  "FdaffE799rVb3oyAuhJ2mBW/XJwD07Uajb2G6YwSAEw=";

/* SwehackBackup */
static const char kSwehackBackupFingerprint[] =
  "z6cuswA6E1vgFkCjUsbEYo0Lf3aP8M8YOvwkoiGzDCo=";

/* TestSPKI */
static const char kTestSPKIFingerprint[] =
  "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";

/* Tor1 */
static const char kTor1Fingerprint[] =
  "bYz9JTDk89X3qu3fgswG+lBQso5vI0N1f0Rx4go4nLo=";

/* Tor2 */
static const char kTor2Fingerprint[] =
  "xXCxhTdn7uxXneJSbQCqoAvuW3ZtQl2pDVTf2sewS8w=";

/* Tor3 */
static const char kTor3Fingerprint[] =
  "CleC1qwUR8JPgH1nXvSe2VHxDe5/KfNs96EusbfSOfo=";

/* TumblrBackup */
static const char kTumblrBackupFingerprint[] =
  "avlD96PLERV78IN1fD+ab5cupkUDD9wTZWJjHX6VC9w=";

/* Twitter1 */
static const char kTwitter1Fingerprint[] =
  "vU9M48LzD/CF34wE5PPf4nBwRyosy06X21J0ap8yS5s=";

/* USERTrust ECC Certification Authority */
static const char kUSERTrust_ECC_Certification_AuthorityFingerprint[] =
  "ICGRfpgmOUXIWcQ/HXPLQTkFPEFPoDyjvH7ohhQpjzs=";

/* USERTrust RSA Certification Authority */
static const char kUSERTrust_RSA_Certification_AuthorityFingerprint[] =
  "x4QzPSC810K5/cMjb05Qm4k3Bw5zBn4lTdO/nEW/Td4=";

/* UTN USERFirst Email Root CA */
static const char kUTN_USERFirst_Email_Root_CAFingerprint[] =
  "Laj56jRU0hFGRko/nQKNxMf7tXscUsc8KwVyovWZotM=";

/* UTN USERFirst Hardware Root CA */
static const char kUTN_USERFirst_Hardware_Root_CAFingerprint[] =
  "TUDnr0MEoJ3of7+YliBMBVFB4/gJsv5zO7IxD9+YoWI=";

/* UTN USERFirst Object Root CA */
static const char kUTN_USERFirst_Object_Root_CAFingerprint[] =
  "D+FMJksXu28NZT56cOs2Pb9UvhWAOe3a5cJXEd9IwQM=";

/* VeriSign Class 3 Public Primary Certification Authority - G4 */
static const char kVeriSign_Class_3_Public_Primary_Certification_Authority___G4Fingerprint[] =
  "UZJDjsNp1+4M5x9cbbdflB779y5YRBcV6Z6rBMLIrO4=";

/* VeriSign Class 3 Public Primary Certification Authority - G5 */
static const char kVeriSign_Class_3_Public_Primary_Certification_Authority___G5Fingerprint[] =
  "JbQbUG5JMJUoI6brnx0x3vZF6jilxsapbXGVfjhN8Fg=";

/* VeriSign Universal Root Certification Authority */
static const char kVeriSign_Universal_Root_Certification_AuthorityFingerprint[] =
  "lnsM2T/O9/J84sJFdnrpsFp3awZJ+ZZbYpCWhGloaHI=";

/* Verisign Class 1 Public Primary Certification Authority - G3 */
static const char kVerisign_Class_1_Public_Primary_Certification_Authority___G3Fingerprint[] =
  "IgduWu9Eu5pBaii30cRDItcFn2D+/6XK9sW+hEeJEwM=";

/* Verisign Class 2 Public Primary Certification Authority - G3 */
static const char kVerisign_Class_2_Public_Primary_Certification_Authority___G3Fingerprint[] =
  "cAajgxHlj7GTSEIzIYIQxmEloOSoJq7VOaxWHfv72QM=";

/* Verisign Class 3 Public Primary Certification Authority - G3 */
static const char kVerisign_Class_3_Public_Primary_Certification_Authority___G3Fingerprint[] =
  "SVqWumuteCQHvVIaALrOZXuzVVVeS7f4FGxxu6V+es4=";

/* YahooBackup1 */
static const char kYahooBackup1Fingerprint[] =
  "2fRAUXyxl4A1/XHrKNBmc8bTkzA7y4FB/GLJuNAzCqY=";

/* YahooBackup2 */
static const char kYahooBackup2Fingerprint[] =
  "dolnbtzEBnELx/9lOEQ22e6OZO/QNb6VSSX2XHA3E7A=";

/* thawte Primary Root CA */
static const char kthawte_Primary_Root_CAFingerprint[] =
  "HXXQgxueCIU5TTLHob/bPbwcKOKw6DkfsTWYHbxbqTY=";

/* thawte Primary Root CA - G2 */
static const char kthawte_Primary_Root_CA___G2Fingerprint[] =
  "Z9xPMvoQ59AaeaBzqgyeAhLsL/w9d54Kp/nA8OHCyJM=";

/* thawte Primary Root CA - G3 */
static const char kthawte_Primary_Root_CA___G3Fingerprint[] =
  "GQbGEk27Q4V40A4GbVBUxsN/D6YCjAVUXgmU7drshik=";

/* Pinsets are each an ordered list by the actual value of the fingerprint */
struct StaticFingerprints {
  const size_t size;
  const char* const* data;
};

/* PreloadedHPKPins.json pinsets */
static const char* const kPinset_google_root_pems_Data[] = {
  kEntrust_Root_Certification_Authority___EC1Fingerprint,
  kComodo_Trusted_Services_rootFingerprint,
  kCOMODO_ECC_Certification_AuthorityFingerprint,
  kDigiCert_Assured_ID_Root_G2Fingerprint,
  kCOMODO_Certification_AuthorityFingerprint,
  kAddTrust_Low_Value_Services_RootFingerprint,
  kGlobalSign_ECC_Root_CA___R4Fingerprint,
  kGeoTrust_Global_CA_2Fingerprint,
  kDigiCert_Assured_ID_Root_G3Fingerprint,
  kStarfield_Class_2_CAFingerprint,
  kthawte_Primary_Root_CA___G3Fingerprint,
  kthawte_Primary_Root_CAFingerprint,
  kEntrust_net_Premium_2048_Secure_Server_CAFingerprint,
  kDigiCert_Assured_ID_Root_CAFingerprint,
  kUSERTrust_ECC_Certification_AuthorityFingerprint,
  kVeriSign_Class_3_Public_Primary_Certification_Authority___G5Fingerprint,
  kGlobalSign_Root_CAFingerprint,
  kGo_Daddy_Root_Certificate_Authority___G2Fingerprint,
  kAffirmTrust_Premium_ECCFingerprint,
  kAddTrust_Public_Services_RootFingerprint,
  kComodo_Secure_Services_rootFingerprint,
  kGeoTrust_Primary_Certification_AuthorityFingerprint,
  kVerisign_Class_3_Public_Primary_Certification_Authority___G3Fingerprint,
  kUTN_USERFirst_Hardware_Root_CAFingerprint,
  kVeriSign_Class_3_Public_Primary_Certification_Authority___G4Fingerprint,
  kGo_Daddy_Class_2_CAFingerprint,
  kDigiCert_Trusted_Root_G4Fingerprint,
  kDigiCert_High_Assurance_EV_Root_CAFingerprint,
  kBaltimore_CyberTrust_RootFingerprint,
  kthawte_Primary_Root_CA___G2Fingerprint,
  kAffirmTrust_CommercialFingerprint,
  kEntrust_Root_Certification_AuthorityFingerprint,
  kGlobalSign_Root_CA___R3Fingerprint,
  kEntrust_Root_Certification_Authority___G2Fingerprint,
  kGeoTrust_Universal_CA_2Fingerprint,
  kGlobalSign_ECC_Root_CA___R5Fingerprint,
  kCybertrust_Global_RootFingerprint,
  kStarfield_Root_Certificate_Authority___G2Fingerprint,
  kCOMODO_RSA_Certification_AuthorityFingerprint,
  kGeoTrust_Global_CAFingerprint,
  kDigiCert_Global_Root_G2Fingerprint,
  kGlobalSign_Root_CA___R2Fingerprint,
  kAffirmTrust_NetworkingFingerprint,
  kAddTrust_External_RootFingerprint,
  kVeriSign_Universal_Root_Certification_AuthorityFingerprint,
  kGeoTrust_Universal_CAFingerprint,
  kGeoTrust_Primary_Certification_Authority___G3Fingerprint,
  kDigiCert_Global_Root_CAFingerprint,
  kDigiCert_Global_Root_G3Fingerprint,
  kGeoTrust_Primary_Certification_Authority___G2Fingerprint,
  kComodo_AAA_Services_rootFingerprint,
  kAffirmTrust_PremiumFingerprint,
  kUSERTrust_RSA_Certification_AuthorityFingerprint,
  kAddTrust_Qualified_Certificates_RootFingerprint,
};
static const StaticFingerprints kPinset_google_root_pems = {
  sizeof(kPinset_google_root_pems_Data) / sizeof(const char*),
  kPinset_google_root_pems_Data
};

static const char* const kPinset_mozilla_Data[] = {
  kGeoTrust_Global_CA_2Fingerprint,
  kthawte_Primary_Root_CA___G3Fingerprint,
  kthawte_Primary_Root_CAFingerprint,
  kDigiCert_Assured_ID_Root_CAFingerprint,
  kVerisign_Class_1_Public_Primary_Certification_Authority___G3Fingerprint,
  kVeriSign_Class_3_Public_Primary_Certification_Authority___G5Fingerprint,
  kGeoTrust_Primary_Certification_AuthorityFingerprint,
  kVerisign_Class_3_Public_Primary_Certification_Authority___G3Fingerprint,
  kVeriSign_Class_3_Public_Primary_Certification_Authority___G4Fingerprint,
  kDigiCert_High_Assurance_EV_Root_CAFingerprint,
  kBaltimore_CyberTrust_RootFingerprint,
  kthawte_Primary_Root_CA___G2Fingerprint,
  kVerisign_Class_2_Public_Primary_Certification_Authority___G3Fingerprint,
  kGeoTrust_Universal_CA_2Fingerprint,
  kGeoTrust_Global_CAFingerprint,
  kVeriSign_Universal_Root_Certification_AuthorityFingerprint,
  kGeoTrust_Universal_CAFingerprint,
  kGeoTrust_Primary_Certification_Authority___G3Fingerprint,
  kDigiCert_Global_Root_CAFingerprint,
  kGeoTrust_Primary_Certification_Authority___G2Fingerprint,
};
static const StaticFingerprints kPinset_mozilla = {
  sizeof(kPinset_mozilla_Data) / sizeof(const char*),
  kPinset_mozilla_Data
};

static const char* const kPinset_mozilla_services_Data[] = {
  kDigiCert_Global_Root_CAFingerprint,
};
static const StaticFingerprints kPinset_mozilla_services = {
  sizeof(kPinset_mozilla_services_Data) / sizeof(const char*),
  kPinset_mozilla_services_Data
};

static const char* const kPinset_mozilla_test_Data[] = {
  kEnd_Entity_Test_CertFingerprint,
};
static const StaticFingerprints kPinset_mozilla_test = {
  sizeof(kPinset_mozilla_test_Data) / sizeof(const char*),
  kPinset_mozilla_test_Data
};

/* Chrome static pinsets */
static const char* const kPinset_test_Data[] = {
  kTestSPKIFingerprint,
};
static const StaticFingerprints kPinset_test = {
  sizeof(kPinset_test_Data) / sizeof(const char*),
  kPinset_test_Data
};

static const char* const kPinset_google_Data[] = {
  kGOOGLE_PIN_GoogleG2Fingerprint,
  kGoogleBackup2048Fingerprint,
  kGeoTrust_Global_CAFingerprint,
  kGlobalSign_Root_CA___R2Fingerprint,
};
static const StaticFingerprints kPinset_google = {
  sizeof(kPinset_google_Data) / sizeof(const char*),
  kPinset_google_Data
};

static const char* const kPinset_tor_Data[] = {
  kTor3Fingerprint,
  kDigiCert_High_Assurance_EV_Root_CAFingerprint,
  kGOOGLE_PIN_LetsEncryptAuthorityPrimary_X1_X3Fingerprint,
  kTor1Fingerprint,
  kGOOGLE_PIN_RapidSSLFingerprint,
  kGOOGLE_PIN_LetsEncryptAuthorityBackup_X2_X4Fingerprint,
  kTor2Fingerprint,
};
static const StaticFingerprints kPinset_tor = {
  sizeof(kPinset_tor_Data) / sizeof(const char*),
  kPinset_tor_Data
};

static const char* const kPinset_twitterCom_Data[] = {
  kGOOGLE_PIN_VeriSignClass2_G2Fingerprint,
  kGOOGLE_PIN_VeriSignClass3_G2Fingerprint,
  kGeoTrust_Global_CA_2Fingerprint,
  kDigiCert_Assured_ID_Root_CAFingerprint,
  kVerisign_Class_1_Public_Primary_Certification_Authority___G3Fingerprint,
  kVeriSign_Class_3_Public_Primary_Certification_Authority___G5Fingerprint,
  kGOOGLE_PIN_VeriSignClass1Fingerprint,
  kGeoTrust_Primary_Certification_AuthorityFingerprint,
  kVerisign_Class_3_Public_Primary_Certification_Authority___G3Fingerprint,
  kVeriSign_Class_3_Public_Primary_Certification_Authority___G4Fingerprint,
  kGOOGLE_PIN_VeriSignClass4_G3Fingerprint,
  kDigiCert_High_Assurance_EV_Root_CAFingerprint,
  kVerisign_Class_2_Public_Primary_Certification_Authority___G3Fingerprint,
  kGeoTrust_Universal_CA_2Fingerprint,
  kGeoTrust_Global_CAFingerprint,
  kVeriSign_Universal_Root_Certification_AuthorityFingerprint,
  kGeoTrust_Universal_CAFingerprint,
  kGeoTrust_Primary_Certification_Authority___G3Fingerprint,
  kDigiCert_Global_Root_CAFingerprint,
  kGeoTrust_Primary_Certification_Authority___G2Fingerprint,
  kTwitter1Fingerprint,
};
static const StaticFingerprints kPinset_twitterCom = {
  sizeof(kPinset_twitterCom_Data) / sizeof(const char*),
  kPinset_twitterCom_Data
};

static const char* const kPinset_twitterCDN_Data[] = {
  kGOOGLE_PIN_VeriSignClass2_G2Fingerprint,
  kComodo_Trusted_Services_rootFingerprint,
  kCOMODO_Certification_AuthorityFingerprint,
  kGOOGLE_PIN_VeriSignClass3_G2Fingerprint,
  kAddTrust_Low_Value_Services_RootFingerprint,
  kUTN_USERFirst_Object_Root_CAFingerprint,
  kGOOGLE_PIN_GTECyberTrustGlobalRootFingerprint,
  kGeoTrust_Global_CA_2Fingerprint,
  kEntrust_net_Premium_2048_Secure_Server_CAFingerprint,
  kDigiCert_Assured_ID_Root_CAFingerprint,
  kVerisign_Class_1_Public_Primary_Certification_Authority___G3Fingerprint,
  kVeriSign_Class_3_Public_Primary_Certification_Authority___G5Fingerprint,
  kGlobalSign_Root_CAFingerprint,
  kUTN_USERFirst_Email_Root_CAFingerprint,
  kGOOGLE_PIN_VeriSignClass1Fingerprint,
  kAddTrust_Public_Services_RootFingerprint,
  kGOOGLE_PIN_UTNDATACorpSGCFingerprint,
  kComodo_Secure_Services_rootFingerprint,
  kGeoTrust_Primary_Certification_AuthorityFingerprint,
  kVerisign_Class_3_Public_Primary_Certification_Authority___G3Fingerprint,
  kUTN_USERFirst_Hardware_Root_CAFingerprint,
  kVeriSign_Class_3_Public_Primary_Certification_Authority___G4Fingerprint,
  kGOOGLE_PIN_VeriSignClass4_G3Fingerprint,
  kDigiCert_High_Assurance_EV_Root_CAFingerprint,
  kBaltimore_CyberTrust_RootFingerprint,
  kEntrust_Root_Certification_AuthorityFingerprint,
  kVerisign_Class_2_Public_Primary_Certification_Authority___G3Fingerprint,
  kGlobalSign_Root_CA___R3Fingerprint,
  kEntrust_Root_Certification_Authority___G2Fingerprint,
  kGeoTrust_Universal_CA_2Fingerprint,
  kGeoTrust_Global_CAFingerprint,
  kGlobalSign_Root_CA___R2Fingerprint,
  kAddTrust_External_RootFingerprint,
  kVeriSign_Universal_Root_Certification_AuthorityFingerprint,
  kGeoTrust_Universal_CAFingerprint,
  kGOOGLE_PIN_Entrust_SSLFingerprint,
  kGeoTrust_Primary_Certification_Authority___G3Fingerprint,
  kDigiCert_Global_Root_CAFingerprint,
  kGeoTrust_Primary_Certification_Authority___G2Fingerprint,
  kComodo_AAA_Services_rootFingerprint,
  kTwitter1Fingerprint,
  kAddTrust_Qualified_Certificates_RootFingerprint,
};
static const StaticFingerprints kPinset_twitterCDN = {
  sizeof(kPinset_twitterCDN_Data) / sizeof(const char*),
  kPinset_twitterCDN_Data
};

static const char* const kPinset_dropbox_Data[] = {
  kEntrust_Root_Certification_Authority___EC1Fingerprint,
  kEntrust_net_Premium_2048_Secure_Server_CAFingerprint,
  kDigiCert_Assured_ID_Root_CAFingerprint,
  kGo_Daddy_Root_Certificate_Authority___G2Fingerprint,
  kGOOGLE_PIN_GoDaddySecureFingerprint,
  kGo_Daddy_Class_2_CAFingerprint,
  kDigiCert_High_Assurance_EV_Root_CAFingerprint,
  kEntrust_Root_Certification_AuthorityFingerprint,
  kEntrust_Root_Certification_Authority___G2Fingerprint,
  kDigiCert_Global_Root_CAFingerprint,
};
static const StaticFingerprints kPinset_dropbox = {
  sizeof(kPinset_dropbox_Data) / sizeof(const char*),
  kPinset_dropbox_Data
};

static const char* const kPinset_facebook_Data[] = {
  kGOOGLE_PIN_DigiCertECCSecureServerCAFingerprint,
  kDigiCert_High_Assurance_EV_Root_CAFingerprint,
  kGOOGLE_PIN_SymantecClass3EVG3Fingerprint,
  kFacebookBackupFingerprint,
};
static const StaticFingerprints kPinset_facebook = {
  sizeof(kPinset_facebook_Data) / sizeof(const char*),
  kPinset_facebook_Data
};

static const char* const kPinset_spideroak_Data[] = {
  kSpiderOak2Fingerprint,
  kSpiderOak3Fingerprint,
  kDigiCert_High_Assurance_EV_Root_CAFingerprint,
  kGeoTrust_Global_CAFingerprint,
};
static const StaticFingerprints kPinset_spideroak = {
  sizeof(kPinset_spideroak_Data) / sizeof(const char*),
  kPinset_spideroak_Data
};

static const char* const kPinset_yahoo_Data[] = {
  kYahooBackup1Fingerprint,
  kGOOGLE_PIN_VeriSignClass2_G2Fingerprint,
  kDigiCert_Assured_ID_Root_CAFingerprint,
  kVeriSign_Class_3_Public_Primary_Certification_Authority___G5Fingerprint,
  kVerisign_Class_3_Public_Primary_Certification_Authority___G3Fingerprint,
  kVeriSign_Class_3_Public_Primary_Certification_Authority___G4Fingerprint,
  kDigiCert_Trusted_Root_G4Fingerprint,
  kDigiCert_High_Assurance_EV_Root_CAFingerprint,
  kVerisign_Class_2_Public_Primary_Certification_Authority___G3Fingerprint,
  kYahooBackup2Fingerprint,
  kDigiCert_Global_Root_G2Fingerprint,
  kVeriSign_Universal_Root_Certification_AuthorityFingerprint,
  kDigiCert_Global_Root_CAFingerprint,
  kDigiCert_Global_Root_G3Fingerprint,
};
static const StaticFingerprints kPinset_yahoo = {
  sizeof(kPinset_yahoo_Data) / sizeof(const char*),
  kPinset_yahoo_Data
};

static const char* const kPinset_swehackCom_Data[] = {
  kSwehackFingerprint,
  kDST_Root_CA_X3Fingerprint,
  kGOOGLE_PIN_LetsEncryptAuthorityPrimary_X1_X3Fingerprint,
  kGOOGLE_PIN_COMODORSADomainValidationSecureServerCAFingerprint,
  kGOOGLE_PIN_LetsEncryptAuthorityBackup_X2_X4Fingerprint,
  kSwehackBackupFingerprint,
};
static const StaticFingerprints kPinset_swehackCom = {
  sizeof(kPinset_swehackCom_Data) / sizeof(const char*),
  kPinset_swehackCom_Data
};

static const char* const kPinset_ncsccs_Data[] = {
  kCOMODO_ECC_Certification_AuthorityFingerprint,
  kDigiCert_Assured_ID_Root_CAFingerprint,
  kDigiCert_High_Assurance_EV_Root_CAFingerprint,
  kBaltimore_CyberTrust_RootFingerprint,
  kGOOGLE_PIN_LetsEncryptAuthorityPrimary_X1_X3Fingerprint,
  kCOMODO_RSA_Certification_AuthorityFingerprint,
  kAddTrust_External_RootFingerprint,
  kDigiCert_Global_Root_CAFingerprint,
  kGOOGLE_PIN_LetsEncryptAuthorityBackup_X2_X4Fingerprint,
};
static const StaticFingerprints kPinset_ncsccs = {
  sizeof(kPinset_ncsccs_Data) / sizeof(const char*),
  kPinset_ncsccs_Data
};

static const char* const kPinset_tumblr_Data[] = {
  kDigiCert_High_Assurance_EV_Root_CAFingerprint,
  kTumblrBackupFingerprint,
  kGOOGLE_PIN_DigiCertSHA2HighAssuranceServerCAFingerprint,
};
static const StaticFingerprints kPinset_tumblr = {
  sizeof(kPinset_tumblr_Data) / sizeof(const char*),
  kPinset_tumblr_Data
};

/* Domainlist */
struct TransportSecurityPreload {
  const char* mHost;
  const bool mIncludeSubdomains;
  const bool mTestMode;
  const bool mIsMoz;
  const int32_t mId;
  const StaticFingerprints* pinset;
};

/* Sort hostnames for binary search. */
static const TransportSecurityPreload kPublicKeyPinningPreloadList[] = {
  { "exclude-subdomains.pinning.example.com", false, false, false, 0, &kPinset_mozilla_test },
  { "include-subdomains.pinning.example.com", true, false, false, -1, &kPinset_mozilla_test },
  { "test-mode.pinning.example.com", true, true, false, -1, &kPinset_mozilla_test },
};

// Pinning Preload List Length = 3;

static const int32_t kUnknownId = -1;

static const PRTime kPreloadPKPinsExpirationTime = INT64_C(1609459199000000);
