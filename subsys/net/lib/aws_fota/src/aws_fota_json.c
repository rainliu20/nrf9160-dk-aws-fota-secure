/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <string.h>
#include <cJSON.h>
#include <sys/util.h>
#include <net/aws_jobs.h>

#include "aws_fota_json.h"

/**@brief Copy max maxlen bytes from src to dst. Insert null-terminator.
 */
static void strncpy_nullterm(char *dst, const char *src, size_t maxlen)
{
	size_t len = strlen(src) + 1;

	memcpy(dst, src, MIN(len, maxlen));
	if (len > maxlen) {
		dst[maxlen - 1] = '\0';
	}
}

int aws_fota_parse_UpdateJobExecution_rsp(const char *update_rsp_document,
					  size_t payload_len, char *status_buf)
{
	if (update_rsp_document == NULL || status_buf == NULL) {
		return -EINVAL;
	}

	int ret;

	cJSON *update_response = cJSON_Parse(update_rsp_document);

	if (update_response == NULL) {
		ret = -ENODATA;
		goto cleanup;
	}

	cJSON *status = cJSON_GetObjectItemCaseSensitive(update_response,
							  "status");
	if (cJSON_IsString(status) && status->valuestring != NULL) {
		strncpy_nullterm(status_buf, status->valuestring,
				 STATUS_MAX_LEN);
	} else {
		ret = -ENODATA;
		goto cleanup;
	}

	ret = 0;
cleanup:
	cJSON_Delete(update_response);
	return ret;
}

int aws_fota_parse_DescribeJobExecution_rsp(const char *job_document,
					   u32_t payload_len,
					   char *job_id_buf,
					   char *schema_buf,
					   char *hostname_buf,
					   char *file_path_buf,
					   int *execution_version_number)
{
	if (job_document == NULL
	    || job_id_buf == NULL
	    || schema_buf == NULL
	    || hostname_buf == NULL
	    || file_path_buf == NULL
	    || execution_version_number == NULL) {
		return -EINVAL;
	}

	int ret;

	cJSON *json_data = cJSON_Parse(job_document);

	if (json_data == NULL) {
		ret = -ENODATA;
		goto cleanup;
	}

	cJSON *execution = cJSON_GetObjectItemCaseSensitive(json_data,
							    "execution");
	if (execution == NULL) {
		ret = 0;
		goto cleanup;
	}

	cJSON *job_id = cJSON_GetObjectItemCaseSensitive(execution, "jobId");

	if (cJSON_GetStringValue(job_id) != NULL) {
		strncpy_nullterm(job_id_buf, job_id->valuestring,
				AWS_JOBS_JOB_ID_MAX_LEN);
	} else {
		ret = -ENODATA;
		goto cleanup;
	}

	cJSON *job_data = cJSON_GetObjectItemCaseSensitive(execution,
							   "jobDocument");

	if (!cJSON_IsObject(job_data)) {
		ret = -ENODATA;
		goto cleanup;
	}

	cJSON *location = cJSON_GetObjectItemCaseSensitive(job_data,
							    "location");

	if (!cJSON_IsObject(location)) {
		ret = -ENODATA;
		goto cleanup;
	}

	cJSON *hostname = cJSON_GetObjectItemCaseSensitive(location, "host");
	cJSON *path = cJSON_GetObjectItemCaseSensitive(location, "path");
	cJSON *protocol = cJSON_GetObjectItemCaseSensitive(location, "protocol");

#if defined(CONFIG_HTTP_PARSER_URL)
	cJSON *url = cJSON_GetObjectItemCaseSensitive(location, "url");

	if (cJSON_GetStringValue(url) != NULL) {
		struct http_parser_url u;
		http_parser_url_init(&u);
		http_parser_parse_url(url->valuestring, strlen(url->valuestring), false, &u);
		/* TODO: Check len vs max len */
		strncpy_nullterm(schema_buf, url->valuestring
				 + u.field_data[UF_SCHEMA].off,
				 u.field_data[UF_SCHEMA].len + 1);
		strncpy_nullterm(hostname_buf, url->valuestring
				 + u.field_data[UF_HOST].off,
				 u.field_data[UF_HOST].len + 1);
		strncpy_nullterm(file_path_buf, url->valuestring
				 + u.field_data[UF_PATH].off + 1,
				 u.field_data[UF_PATH].len
				 + u.field_data[UF_QUERY].len + 1);
	} else
#endif
	/* This is kept for backwards compatibility with previous format */

	if ((cJSON_GetStringValue(hostname) != NULL)
	   && (cJSON_GetStringValue(path) != NULL)
	   && (cJSON_GetStringValue(protocol) != NULL)) {
		strncpy_nullterm(schema_buf, protocol->valuestring, 8);
		strncpy_nullterm(hostname_buf, hostname->valuestring,
				CONFIG_AWS_FOTA_HOSTNAME_MAX_LEN);
		strncpy_nullterm(file_path_buf, path->valuestring,
				CONFIG_AWS_FOTA_FILE_PATH_MAX_LEN);
				
	} else {
		ret = -ENODATA;
		goto cleanup;
	}

	cJSON *version_number = cJSON_GetObjectItemCaseSensitive(
					execution, "versionNumber");

	if (cJSON_IsNumber(version_number)) {
		*execution_version_number = version_number->valueint;
	} else {
		ret = -ENODATA;
		goto cleanup;
	}

	ret = 1;
cleanup:
	cJSON_Delete(json_data);
	return ret;
}
