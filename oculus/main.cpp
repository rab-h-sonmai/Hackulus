#define _CRT_SECURE_NO_DEPRECATE

#include <Windows.h>
#include <shlobj.h> // for SHGetFolderPathA
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h> // for _getch

#include "sqlite3.h"

#pragma comment(lib,"shlwapi.lib")

#define SQL_ALIAS_SELECT "SELECT length(value) as blobLength, value FROM Objects WHERE typename = \"User\""
#define SQL_APPS_SELECT  "SELECT length(value) as blobLength, value FROM \"Objects\" WHERE \"hashkey\" = \"__VIEWER_PRIMARY_KEY__\""
#define SQL_APPS_UPDATE  "UPDATE Objects SET value=:Blob WHERE \"hashkey\" = \"__VIEWER_PRIMARY_KEY__\""

#define BLOB_APPS_KEY    "are_unknown_applications_allowed"
#define BLOB_SOURCES_KEY "oculus_unknown_sources"
#define BLOB_ALIAS_KEY   "alias"

// Get the alias of the session profile to display it later
static const char* getAlias(sqlite3 *db)
{
	int          offset  = 0;
	char        *message = nullptr;
	static char  result[1024];

	result[0] = 0;

	sqlite3_stmt *prepared = nullptr;
	if (sqlite3_prepare_v2(db, SQL_ALIAS_SELECT, -1, &prepared, nullptr) != SQLITE_OK)
	{
		fprintf(stderr, "[getAlias] Could not prepare SQL : %s\n", message);
		goto done;
	}

	int stepResult = sqlite3_step(prepared);
	if (stepResult != SQLITE_ROW && stepResult != SQLITE_DONE)
	{
		fprintf(stderr, "[getAlias] Could not step SQL: %s\n", message);
		goto done;
	}

	size_t      length = sqlite3_column_int(prepared, 0);
	const char *blob   = (const char*)sqlite3_column_blob(prepared, 1);
	if (!blob)
	{
		fprintf(stderr, "[getAlias] Could not read BLOB data\n");
		goto done;
	}

	while (offset < length - strlen(BLOB_ALIAS_KEY))
	{
		if (strncmp(blob + offset, BLOB_ALIAS_KEY, strlen(BLOB_ALIAS_KEY)) == 0)
		{
			offset += strlen(BLOB_ALIAS_KEY);
			offset++;    //skip a '1', one record?
			offset++;    //skip the next '1', urr... one value?

			unsigned long long aliasLength = *((unsigned long long*)(blob + offset));
			offset += 8; //skip the length
			strncpy(result, blob + offset, aliasLength);
			result[aliasLength] = 0;
			break;
		}
		offset++;
	}

done:
	if (prepared)
		sqlite3_finalize(prepared);

	return result;
}

// Save the BLOB the first time this program is run, just in case anything goes wrong.
static int backupOriginal(const char *filepath, const char *blob, size_t length)
{
	char name[MAX_PATH];

	sprintf(name, "%s.blob", filepath);
	if (GetFileAttributesA(name) != INVALID_FILE_ATTRIBUTES) // the file already exists, we're only creating it once
		return 1;

	printf("Creating a backup of the blob called %s...\n", name);

	FILE *file = fopen(name, "wb");
	if (!file)
	{
		fprintf(stderr, "[backupOriginal] Could not create backup file.\n");
		return 0;
	}

	size_t written = fwrite(blob, 1, length, file);
	fclose(file);

	if (written != length)
	{
		fprintf(stderr, "[backupOriginal] Could not save backup file.\n");
		return 0;
	}

	printf("Done.\n");
	return 1;
}

// Open the SQLite database, extract the BLOB, try change the flag, finally try update the database.
static int processSession(const char *filepath)
{
	sqlite3 *db             = nullptr;
	char    *message        = nullptr; // db error messages
	char    *result         = nullptr; // updated BLOB
	int      updateRequired = 0;
	size_t   resultLength   = 0;

	// open the DB
	// -------------------------------------------------------------------------------------
	if (sqlite3_open(filepath, &db))
	{
		fprintf(stderr, "[processSession] Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		goto done;
	}

	// get the username for asking if we wish to toggle the value
	// -------------------------------------------------------------------------------------
	const char *alias = getAlias(db);
	if (!strlen(alias))
	{
		fprintf(stderr, "[processSession] Could not obtain alias, using %s instead.\n", filepath);
		alias = filepath;
	}

	printf("Processing profile for '%s'\n", alias);

	// get the source BLOB
	// -------------------------------------------------------------------------------------
	sqlite3_stmt *prepared = nullptr;
	if (sqlite3_prepare_v2(db, SQL_APPS_SELECT, -1, &prepared, nullptr) != SQLITE_OK)
	{
		fprintf(stderr, "[processSession] SQL error: %s\n", message);
		goto done;
	}
	if (sqlite3_step(prepared) != SQLITE_ROW)
	{
		fprintf(stderr, "[processSession] SQL error: %s\n", message);
		goto done;
	}

	size_t      sourceLength = sqlite3_column_int(prepared, 0);
	const char *source       = (const char*)sqlite3_column_blob(prepared, 1);

	if (!backupOriginal(filepath, source, sourceLength))
	{
		printf("Could not create backup, continue anyway (Y/N)?\n");
		fflush(stdin);
		if (toupper(_getch()) != 'Y')
			return -1;
	}

	result = new char[sourceLength + 3]; //at most we'll add three (7-4) bytes to the value of the field
	memset(result, 0, sourceLength + 3);
	// edit the BLOB to suit our settings
	// -------------------------------------------------------------------------------------
	{
		int offset = 0;
		while (offset < sourceLength)
		{
			if (offset > strlen(BLOB_APPS_KEY) && strncmp(source + offset - strlen(BLOB_APPS_KEY), BLOB_APPS_KEY, strlen(BLOB_APPS_KEY)) == 0)
			{
				result[resultLength++] = source[offset++]; //skip the 1
				result[resultLength++] = source[offset++]; //skip another 1
				int currentValue       = source[offset++]; //the actual 0/1 value
				printf("\tUnknown applications are currently %s. Toggle for this profile (Y/N)?\n", currentValue ? "allowed" : "NOT allowed");
				fflush(stdin);
				if (toupper(_getch()) == 'Y')
				{
					currentValue   = !currentValue;
					updateRequired = 1;
				}
				result[resultLength++] = currentValue; 
			}
#pragma region UNUSED_SOURCES
#if 0
			else if (offset > strlen(BLOB_SOURCES_KEY) && strncmp(source + offset - strlen(BLOB_SOURCES_KEY), BLOB_SOURCES_KEY, strlen(BLOB_SOURCES_KEY)) == 0)
			{
				result[resultLength++] = source[offset++];         //skip what I assume is the type, in this case 's' for string?
				memcpy(result + resultLength, source + offset, 4); //skip the 4-byte int length of the field name
				resultLength += 4; offset += 4;
				memcpy(result + resultLength, source + offset, 5); //which in this case is "value"
				resultLength += 5; offset += 5;
				result[resultLength++] = source[offset++];         //skip a '1', one record?
				result[resultLength++] = source[offset++];         //skip the next '1', urr... one value?

				// now we're at the length of the value. it seems to be an 8-byte int, but we only care about '4'='true' or '7'='unknown', sooo...
				char currentValueLength = *(source + offset);
				char newValueLength     = currentValueLength;
				printf("\tExternal sources are currently %s. Toggle for this profile (Y/N)?\n", currentValueLength == 4 ? "allowed" : "NOT allowed");
				fflush(stdin);
				if (toupper(_getch()) == 'Y')
				{
					newValueLength = currentValueLength == 7 ? 4 : 7;
					updateRequired = 1;
				}

				result[resultLength] = newValueLength;
				resultLength += 8; // skip the length value
				offset += 8; // skip the length value

				char *value = nullptr;
				if (newValueLength == 4)
					value = "true";
				else if (newValueLength == 7)
					value = "unknown";

				memcpy(result + resultLength, value, strlen(value));
				resultLength += strlen(value);
				offset += currentValueLength;
			}
#endif
#pragma endregion
			else
			{
				result[resultLength++] = source[offset++];
			}
		}
	}
	sqlite3_finalize(prepared);

	// store the updated BLOB
	// -------------------------------------------------------------------------------------
	if (resultLength)
	{
		if (sqlite3_prepare_v2(db, SQL_APPS_UPDATE, -1, &prepared, nullptr) != SQLITE_OK)
		{
			fprintf(stderr, "[processSession] SQL error while preparing UPDATE: %s\n", message);
			goto done;
		}
		if (sqlite3_bind_blob(prepared, 1, result, resultLength, SQLITE_TRANSIENT) != SQLITE_OK)
		{
			fprintf(stderr, "[processSession] SQL error while binding BLOB: %s\n", message);
			goto done;
		}
		int stepResult = sqlite3_step(prepared);
		if (stepResult != SQLITE_ROW && stepResult != SQLITE_DONE)
		{
			fprintf(stderr, "[processSession] SQL error while executing UPDATE: %s\n", message);
			goto done;
		}
	}
	else
	{
		fprintf(stderr, "[processSession] Nothing to do.\n");
	}
done:
	delete[] result;
	sqlite3_close(db);
	if (message)
		sqlite3_free(message);
	return 0;
}

// Go through every folder in the session folder and try change the database within it.
static int enumerateSessions()
{
	WIN32_FIND_DATAA data;
	HANDLE           handle;
	char             folder[MAX_PATH];

	if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, folder)))
	{
		fprintf(stderr, "[enumerateSessions] Could not obtain APPDATA folder(%d), cannot continue.", GetLastError());
		_getch();
		return -1;
	}

	strcat(folder, "\\Oculus\\sessions\\*.*");

	handle = FindFirstFileA(folder, &data);
	if (handle == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "[enumerateSessions] FindFirstFile failed (%d), cannot continue.", GetLastError());
		_getch();
		return -2;
	}

	folder[strlen(folder) - 3] = 0; // get rid of *.*
	do
	{
		if (((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) && (isdigit(data.cFileName[0])))
		{
			char sqlPath[MAX_PATH];

			sprintf(sqlPath, "%s%s\\data.sqlite", folder, data.cFileName);
			processSession(sqlPath);
		}
	}
	while (FindNextFileA(handle, &data));
	FindClose(handle);
	return 0;
}

int main(int argc, char *argv[])
{
	SC_HANDLE      servicesDepartment = nullptr;
	SC_HANDLE      oculusService      = nullptr;
	SERVICE_STATUS status;
	int            result             = 0;
	int            wasRunning         = 0;

	// if we should, try stop the Oculus service.
	// -------------------------------------------------------------------------------------
	if ((argc == 1) || (argc > 1 && strcmp(argv[1], "-skipservice") != 0))
	{
		printf("Querying service...\n");
		servicesDepartment = OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
		oculusService      = servicesDepartment ? OpenServiceA(servicesDepartment, "OVRService", SERVICE_ALL_ACCESS) : nullptr;
		if (!servicesDepartment)
			goto serviceError;

		if (QueryServiceStatus(oculusService, &status) != TRUE)
			goto serviceError;

		if (SERVICE_STOPPED != status.dwCurrentState)
		{
			printf("Trying to stop service...\n");
			wasRunning = 1;
			if (ControlService(oculusService, SERVICE_CONTROL_STOP, &status))
			{
				do
				{
					QueryServiceStatus(oculusService, &status);
					Sleep(100); // oh yeaaah
				}
				while (SERVICE_STOPPED != status.dwCurrentState);
			}
			else
			{
				CloseServiceHandle(servicesDepartment);
				goto serviceError;
			}
		}
	serviceError:
		DWORD error = GetLastError();
		if (error)
		{
			fprintf(stderr, "Could not stop the Oculus service (%d), please rerun this with Administrator privileges, or stop the 'Oculus VR Runtime Service' yourself and rerun this program with the '-skipservice' parameter.\n", GetLastError());
			_getch();
			return 5;
		}
	}
	

	// process the Oculus DB
	// -------------------------------------------------------------------------------------
	result = enumerateSessions();


	// restart the service if we should
	// -------------------------------------------------------------------------------------
	if (wasRunning)
	{
		printf("Trying to start service...\n");
		if (!StartService(oculusService, 0, nullptr))
		{
			int lastError = GetLastError();
			if (lastError && lastError != ERROR_SERVICE_ALREADY_RUNNING)
			{
				fprintf(stderr, "Could not start the 'Oculus VR Runtime Service', please do so manually.\n");
			}
		}
	}

	if (oculusService)
		CloseServiceHandle(oculusService);
	if (servicesDepartment)
		CloseServiceHandle(servicesDepartment);

	printf("\nDone.\n");
	fflush(stdin);
	_getch();
	return result;
}
