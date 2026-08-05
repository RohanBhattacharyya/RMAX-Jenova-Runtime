/*-------------------------------------------------------------+
|                                                              |
|                   _________   ______ _    _____              |
|                  / / ____/ | / / __ \ |  / /   |             |
|             __  / / __/ /  |/ / / / / | / / /| |             |
|            / /_/ / /___/ /|  / /_/ /| |/ / ___ |             |
|            \____/_____/_/ |_/\____/ |___/_/  |_|             |
|                                                              |
|                        Jenova Runtime                        |
|                   Developed by Hamid.Memar                   |
|                                                              |
+-------------------------------------------------------------*/

/*
	Web Platform Stubs

	libcurl and libarchive back the Package Manager and the resource pack extractor, both
	of which are editor-side features: an exported game in a browser never downloads a
	toolchain or unpacks an archive from disk. Building either for WebAssembly would add
	megabytes for code that cannot run, but the symbols still have to resolve, because a
	side module with a dangling import fails the moment anything reaches it.

	These definitions keep the link honest and fail loudly if they are ever called.
*/

#include "Jenova.hpp"

#ifdef TARGET_PLATFORM_WEB

#include <Archive/archive.h>
#include <Archive/archive_entry.h>
#include <Curl/curl.h>

static void ReportUnavailable(const char* featureName)
{
	// A warning, not an error: reaching one of these on the Web is expected, and the caller
	// reports what it could not do.
	jenova::Warning("Jenova Web Runtime", "`%s` is not available on the Web platform.", featureName);
}

extern "C"
{
	// libcurl
	CURL* curl_easy_init(void) { ReportUnavailable("Network Downloads"); return nullptr; }
	CURLcode curl_easy_setopt(CURL*, CURLoption, ...) { return CURLE_NOT_BUILT_IN; }
	CURLcode curl_easy_getinfo(CURL*, CURLINFO, ...) { return CURLE_NOT_BUILT_IN; }
	CURLcode curl_easy_perform(CURL*) { return CURLE_NOT_BUILT_IN; }
	void curl_easy_cleanup(CURL*) {}
	const char* curl_easy_strerror(CURLcode) { return "Unavailable on Web"; }
	CURLcode curl_global_init(long) { return CURLE_NOT_BUILT_IN; }
	void curl_global_cleanup(void) {}

	// libarchive :: Reading
	struct archive* archive_read_new(void) { ReportUnavailable("Archive Extraction"); return nullptr; }
	int archive_read_support_filter_all(struct archive*) { return ARCHIVE_FATAL; }
	int archive_read_support_format_all(struct archive*) { return ARCHIVE_FATAL; }
	int archive_read_open_filename(struct archive*, const char*, size_t) { return ARCHIVE_FATAL; }
	int archive_read_open_memory(struct archive*, const void*, size_t) { return ARCHIVE_FATAL; }
	int archive_read_next_header(struct archive*, struct archive_entry**) { return ARCHIVE_FATAL; }
	int archive_read_data_block(struct archive*, const void**, size_t*, la_int64_t*) { return ARCHIVE_FATAL; }
	int archive_read_data_skip(struct archive*) { return ARCHIVE_FATAL; }
	int archive_read_close(struct archive*) { return ARCHIVE_FATAL; }
	int archive_read_free(struct archive*) { return ARCHIVE_FATAL; }

	// libarchive :: Writing
	struct archive* archive_write_disk_new(void) { return nullptr; }
	int archive_write_disk_set_options(struct archive*, int) { return ARCHIVE_FATAL; }
	int archive_write_disk_set_standard_lookup(struct archive*) { return ARCHIVE_FATAL; }
	int archive_write_header(struct archive*, struct archive_entry*) { return ARCHIVE_FATAL; }
	la_ssize_t archive_write_data_block(struct archive*, const void*, size_t, la_int64_t) { return ARCHIVE_FATAL; }
	int archive_write_finish_entry(struct archive*) { return ARCHIVE_FATAL; }
	int archive_write_close(struct archive*) { return ARCHIVE_FATAL; }
	int archive_write_free(struct archive*) { return ARCHIVE_FATAL; }

	// libarchive :: Entries
	__LA_MODE_T archive_entry_filetype(struct archive_entry*) { return 0; }
	const char* archive_entry_pathname(struct archive_entry*) { return ""; }
	void archive_entry_set_pathname(struct archive_entry*, const char*) {}
	la_int64_t archive_entry_size(struct archive_entry*) { return 0; }
	const char* archive_error_string(struct archive*) { return "Unavailable on Web"; }
	const char* archive_liblzma_version(void) { return "0.0.0"; }
}

#endif
