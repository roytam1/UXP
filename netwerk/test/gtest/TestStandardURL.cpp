#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h" // For MOZ_GTEST_BENCH

#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsIURL.h"
#include "nsIStandardURL.h"
#include "nsString.h"
#include "nsComponentManagerUtils.h"
#include "nsIIPCSerializableURI.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/Unused.h"
#include "nsSerializationHelper.h"
#include "mozilla/Base64.h"
#include "nsEscape.h"

TEST(TestStandardURL, Simple) {
    nsCOMPtr<nsIURL> url( do_CreateInstance(NS_STANDARDURL_CONTRACTID) );
    ASSERT_TRUE(url);
    ASSERT_EQ(url->SetSpec(NS_LITERAL_CSTRING("http://example.com")), NS_OK);

    nsAutoCString out;

    ASSERT_EQ(url->GetSpec(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://example.com/"));

    ASSERT_EQ(url->Resolve(NS_LITERAL_CSTRING("foo.html?q=45"), out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("http://example.com/foo.html?q=45"));

    ASSERT_EQ(url->SetScheme(NS_LITERAL_CSTRING("foo")), NS_OK);

    ASSERT_EQ(url->GetScheme(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("foo"));

    ASSERT_EQ(url->GetHost(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("example.com"));
    ASSERT_EQ(url->SetHost(NS_LITERAL_CSTRING("www.yahoo.com")), NS_OK);
    ASSERT_EQ(url->GetHost(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("www.yahoo.com"));

    ASSERT_EQ(url->SetPath(NS_LITERAL_CSTRING("/some-path/one-the-net/about.html?with-a-query#for-you")), NS_OK);
    ASSERT_EQ(url->GetPath(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("/some-path/one-the-net/about.html?with-a-query#for-you"));

    ASSERT_EQ(url->SetQuery(NS_LITERAL_CSTRING("a=b&d=c&what-ever-you-want-to-be-called=45")), NS_OK);
    ASSERT_EQ(url->GetQuery(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("a=b&d=c&what-ever-you-want-to-be-called=45"));

    ASSERT_EQ(url->SetRef(NS_LITERAL_CSTRING("#some-book-mark")), NS_OK);
    ASSERT_EQ(url->GetRef(out), NS_OK);
    ASSERT_TRUE(out == NS_LITERAL_CSTRING("some-book-mark"));
}

#define COUNT 10000

MOZ_GTEST_BENCH(TestStandardURL, Perf, [] {
    nsCOMPtr<nsIURL> url( do_CreateInstance(NS_STANDARDURL_CONTRACTID) );
    ASSERT_TRUE(url);
    nsAutoCString out;

    for (int i = COUNT; i; --i) {
        ASSERT_EQ(url->SetSpec(NS_LITERAL_CSTRING("http://example.com")), NS_OK);
        ASSERT_EQ(url->GetSpec(out), NS_OK);
        url->Resolve(NS_LITERAL_CSTRING("foo.html?q=45"), out);
        url->SetScheme(NS_LITERAL_CSTRING("foo"));
        url->GetScheme(out);
        url->SetHost(NS_LITERAL_CSTRING("www.yahoo.com"));
        url->GetHost(out);
        url->SetPath(NS_LITERAL_CSTRING("/some-path/one-the-net/about.html?with-a-query#for-you"));
        url->GetPath(out);
        url->SetQuery(NS_LITERAL_CSTRING("a=b&d=c&what-ever-you-want-to-be-called=45"));
        url->GetQuery(out);
        url->SetRef(NS_LITERAL_CSTRING("#some-book-mark"));
        url->GetRef(out);
    }
});

TEST(TestStandardURL, Deserialize_Bug1392739)
{
  mozilla::ipc::StandardURLParams standard_params;
  standard_params.urlType() = nsIStandardURL::URLTYPE_STANDARD;
  standard_params.spec() = NS_LITERAL_CSTRING("");
  standard_params.host() = mozilla::ipc::StandardURLSegment(4294967295, 1);

  mozilla::ipc::URIParams params(standard_params);

  nsCOMPtr<nsIIPCSerializableURI> url = do_CreateInstance(NS_STANDARDURL_CID);
  ASSERT_EQ(url->Deserialize(params), false);
}

TEST(TestStandardURL, CorruptSerialization)
{
  auto spec = "http://user:pass@example.com/path/to/file.ext?query#hash"_ns;

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_MutateURI(NS_STANDARDURLMUTATOR_CONTRACTID)
                    .SetSpec(spec)
                    .Finalize(uri);
  ASSERT_EQ(rv, NS_OK);

  nsAutoCString serialization;
  nsCOMPtr<nsISerializable> serializable = do_QueryInterface(uri);
  ASSERT_TRUE(serializable);

  // Check that the URL is normally serializable.
  ASSERT_EQ(NS_OK, NS_SerializeToString(serializable, serialization));
  nsCOMPtr<nsISupports> deserializedObject;
  ASSERT_EQ(NS_OK, NS_DeserializeObject(serialization,
                                        getter_AddRefs(deserializedObject)));

  nsAutoCString canonicalBin;
  Unused << Base64Decode(serialization, canonicalBin);

// The spec serialization begins at byte 49
// If the implementation of nsStandardURL::Write changes, this test will need
// to be adjusted.
#define SPEC_OFFSET 49

  ASSERT_EQ(Substring(canonicalBin, SPEC_OFFSET, 7), "http://"_ns);

  nsAutoCString corruptedBin = canonicalBin;
  // change mScheme.mPos
  corruptedBin.BeginWriting()[SPEC_OFFSET + spec.Length()] = 1;
  Unused << Base64Encode(corruptedBin, serialization);
  ASSERT_EQ(
      NS_ERROR_MALFORMED_URI,
      NS_DeserializeObject(serialization, getter_AddRefs(deserializedObject)));

  corruptedBin = canonicalBin;
  // change mScheme.mLen
  corruptedBin.BeginWriting()[SPEC_OFFSET + spec.Length() + 4] = 127;
  Unused << Base64Encode(corruptedBin, serialization);
  ASSERT_EQ(
      NS_ERROR_MALFORMED_URI,
      NS_DeserializeObject(serialization, getter_AddRefs(deserializedObject)));
}