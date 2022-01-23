/*
   Copyright (c) 2001, Loki software, inc.
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

   Redistributions of source code must retain the above copyright notice, this list
   of conditions and the following disclaimer.

   Redistributions in binary form must reproduce the above copyright notice, this
   list of conditions and the following disclaimer in the documentation and/or
   other materials provided with the distribution.

   Neither the name of Loki software nor the names of its contributors may be used
   to endorse or promote products derived from this software without specific prior
   written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
   DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//
// Rules:
//
// - Directories should be searched in the following order: ~/.q3a/baseq3,
//   install dir (/usr/local/games/quake3/baseq3) and cd_path (/mnt/cdrom/baseq3).
//
// - Pak files are searched first inside the directories.
// - Case insensitive.
// - Unix-style slashes (/) (windows is backwards .. everyone knows that)
//
// Leonardo Zide (leo@lokigames.com)
//

#include "vfs.h"
#include "globaldefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>

#include "qerplugin.h"
#include "idatastream.h"
#include "iarchive.h"

ArchiveModules &FileSystemQ3API_getArchiveModules();

#include "ifilesystem.h"

#include "generic/callback.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "os/path.h"
#include "moduleobservers.h"
#include "filematch.h"
#include "dpkdeps.h"


const int VFS_MAXDIRS = 64;

#if GDEF_OS_WINDOWS
#define PATH_MAX 260
#endif

#define gamemode_get GlobalRadiant().getGameMode



// =============================================================================
// Global variables

Archive *OpenArchive(const char *name);

struct archive_entry_t {
	CopiedString name;
	Archive *archive;
	bool is_pakfile;
};

#include <list>
#include <map>

typedef std::list<archive_entry_t> archives_t;

static archives_t g_archives;
static char g_strDirs[VFS_MAXDIRS][PATH_MAX + 1];
static int g_numDirs;
static char g_strForbiddenDirs[VFS_MAXDIRS][PATH_MAX + 1];
static int g_numForbiddenDirs = 0;
static bool g_bUsePak = true;

ModuleObservers g_observers;

// =============================================================================
// Static functions

static void AddSlash(char *str)
{
	std::size_t n = strlen(str);
	if (n > 0) {
		if (str[n - 1] != '\\' && str[n - 1] != '/') {
			globalErrorStream() << "WARNING: directory path does not end with separator: " << str << "\n";
			strcat(str, "/");
		}
	}
}

static void FixDOSName(char *src)
{
	if (src == 0 || strchr(src, '\\') == 0) {
		return;
	}

	globalErrorStream() << "WARNING: invalid path separator '\\': " << src << "\n";

	while (*src) {
		if (*src == '\\') {
			*src = '/';
		}
		src++;
	}
}

const _QERArchiveTable *GetArchiveTable(ArchiveModules &archiveModules, const char *ext)
{
	StringOutputStream tmp(16);
	tmp << LowerCase(ext);
	return archiveModules.findModule(tmp.c_str());
}

static Archive *InitPakFile(ArchiveModules &archiveModules, const char *filename)
{
	const _QERArchiveTable *table = GetArchiveTable(archiveModules, path_get_extension(filename));

	if (table != 0) {
		archive_entry_t entry;
		entry.name = filename;

		entry.archive = table->m_pfnOpenArchive(filename);
		entry.is_pakfile = true;
		g_archives.push_back(entry);
		globalOutputStream() << "  " << path_get_extension(filename) << " file: " << filename << "\n";

		return entry.archive;
	}

	return 0;
}

static Archive *InitWadFile(ArchiveModules &archiveModules, const char *filename)
{
	const _QERArchiveTable *table = GetArchiveTable(archiveModules, path_get_extension(filename));

	if (table != 0) {
		archive_entry_t entry;
		entry.name = filename;

		entry.archive = table->m_pfnOpenArchive(filename);
		entry.is_pakfile = false;
		g_archives.push_back(entry);
		globalOutputStream() << "  wad file: " << filename << "\n";

		return entry.archive;
	}

	return 0;
}

inline void pathlist_prepend_unique(GSList *&pathlist, char *path)
{
	if (g_slist_find_custom(pathlist, path, (GCompareFunc) path_compare) == 0) {
		pathlist = g_slist_prepend(pathlist, path);
	} else {
		g_free(path);
	}
}

class DirectoryListVisitor : public Archive::Visitor {
GSList *&m_matches;
const char *m_directory;
public:
DirectoryListVisitor(GSList *&matches, const char *directory)
	: m_matches(matches), m_directory(directory)
{
}

void visit(const char *name)
{
	const char *subname = path_make_relative(name, m_directory);
	if (subname != name) {
		if (subname[0] == '/') {
			++subname;
		}
		char *dir = g_strdup(subname);
		char *last_char = dir + strlen(dir);
		if (last_char != dir && *(--last_char) == '/') {
			*last_char = '\0';
		}
		pathlist_prepend_unique(m_matches, dir);
	}
}
};

class FileListVisitor : public Archive::Visitor {
GSList *&m_matches;
const char *m_directory;
const char *m_extension;
public:
FileListVisitor(GSList *&matches, const char *directory, const char *extension)
	: m_matches(matches), m_directory(directory), m_extension(extension)
{
}

void visit(const char *name)
{
	const char *subname = path_make_relative(name, m_directory);
	if (subname != name) {
		if (subname[0] == '/') {
			++subname;
		}
		if (m_extension[0] == '*' || extension_equal(path_get_extension(subname), m_extension)) {
			pathlist_prepend_unique(m_matches, g_strdup(subname));
		}
	}
}
};

static GSList *GetListInternal(const char *refdir, const char *ext, bool directories, std::size_t depth)
{
	GSList *files = 0;

	ASSERT_MESSAGE(refdir[strlen(refdir) - 1] == '/', "search path does not end in '/'");

	if (directories) {
		for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
			DirectoryListVisitor visitor(files, refdir);
			(*i).archive->forEachFile(Archive::VisitorFunc(visitor, Archive::eDirectories, depth), refdir);
		}
	} else {
		for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
			FileListVisitor visitor(files, refdir, ext);
			(*i).archive->forEachFile(Archive::VisitorFunc(visitor, Archive::eFiles, depth), refdir);
		}
	}

	files = g_slist_reverse(files);

	return files;
}

inline int ascii_to_upper(int c)
{
	if (c >= 'a' && c <= 'z') {
		return c - ('a' - 'A');
	}
	return c;
}

/*!
   This behaves identically to stricmp(a,b), except that ASCII chars
   [\]^`_ come AFTER alphabet chars instead of before. This is because
   it converts all alphabet chars to uppercase before comparison,
   while stricmp converts them to lowercase.
 */
static int string_compare_nocase_upper(const char *a, const char *b)
{
	for (;;) {
		int c1 = ascii_to_upper(*a++);
		int c2 = ascii_to_upper(*b++);

		if (c1 < c2) {
			return -1; // a < b
		}
		if (c1 > c2) {
			return 1; // a > b
		}
		if (c1 == 0) {
			return 0; // a == b
		}
	}
}

// Arnout: note - sort pakfiles in reverse order. This ensures that
// later pakfiles override earlier ones. This because the vfs module
// returns a filehandle to the first file it can find (while it should
// return the filehandle to the file in the most overriding pakfile, the
// last one in the list that is).

//!\todo Analyse the code in rtcw/q3 to see which order it sorts pak files.
class PakLess {
public:
bool operator()(const CopiedString &self, const CopiedString &other) const
{
	return string_compare_nocase_upper(self.c_str(), other.c_str()) > 0;
}
};

typedef std::set<CopiedString, PakLess> Archives;

Archive *AddPk3Dir(const char *fullpath)
{
	if (g_numDirs == VFS_MAXDIRS) { return 0; }

	strncpy(g_strDirs[g_numDirs], fullpath, PATH_MAX);
	g_strDirs[g_numDirs][PATH_MAX] = '\0';
	g_numDirs++;

	{
		archive_entry_t entry;
		entry.name = fullpath;
		entry.archive = OpenArchive(fullpath);
		entry.is_pakfile = false;
		g_archives.push_back(entry);

		return entry.archive;
	}
}

// for Daemon DPK vfs

Archive *AddDpkDir(const char *fullpath)
{
	return AddPk3Dir(fullpath);
}

struct pakfile_path_t {
	CopiedString fullpath; // full pak dir or pk3dir name
	bool is_pakfile; // defines is it .pk3dir or .pk3 file
};

typedef std::pair<CopiedString, pakfile_path_t> PakfilePathsKV;
typedef std::map<CopiedString, pakfile_path_t> PakfilePaths;  // key must have no extension, only name

static PakfilePaths g_pakfile_paths;

void AddDpkPak(const char *name, const char *fullpath, bool is_pakfile)
{
	pakfile_path_t pakfile_path;
	pakfile_path.fullpath = fullpath;
	pakfile_path.is_pakfile = is_pakfile;
	g_pakfile_paths.insert(PakfilePathsKV(name, pakfile_path));
}

// takes name without ext, returns without ext
static const char *GetLatestDpkPakVersion(const char *name)
{
	const char *maxversion = 0;
	const char *result = 0;
	const char *pakname;
	const char *pakversion;
	int namelen = string_length(name);

	for (PakfilePaths::iterator i = g_pakfile_paths.begin(); i != g_pakfile_paths.end(); ++i) {
		pakname = i->first.c_str();
		if (strncmp(pakname, name, namelen) != 0 || pakname[namelen] != '_') { continue; }
		pakversion = pakname + (namelen + 1);
		if (maxversion == 0 || DpkPakVersionCmp(pakversion, maxversion) > 0) {
			maxversion = pakversion;
			result = pakname;
		}
	}
	return result;
}

// release string after using
static char *GetCurrentMapDpkPakName()
{
	char *mapdir;
	char *mapname;
	int mapnamelen;
	char *result = 0;

	mapname = string_clone(GlobalRadiant().getMapName());
	mapnamelen = string_length(mapname);

	mapdir = strrchr(mapname, '/');
	if (mapdir) {
		mapdir -= 12;
		if (strncmp(mapdir, ".dpkdir/maps/", 13) == 0) {
			*mapdir = '\0';
			mapdir = strrchr(mapname, '/');
			if (mapdir) { mapdir++; }
			else { mapdir = mapname; }
			result = string_clone(mapdir);
		}
	}

	string_release(mapname, mapnamelen);
	return result;

}

// prevent loading duplicates or circular references
static Archives g_loaded_dpk_paks;

// actual pak adding on initialise, deferred from InitDirectory
// Daemon DPK filesystem doesn't need load all paks it finds
static void LoadDpkPakWithDeps(const char *pakname)
{
	Archive *arc;
	ArchiveTextFile *depsFile;

	if (pakname == NULL) {
		// load DEPS from game pack
		StringOutputStream baseDirectory(256);
		const char *basegame = GlobalRadiant().getRequiredGameDescriptionKeyValue("basegame");
		baseDirectory << GlobalRadiant().getGameToolsPath() << basegame << '/';
		arc = AddDpkDir(baseDirectory.c_str());
		depsFile = arc->openTextFile("DEPS");
	} else {
		const char *und = strrchr(pakname, '_');
		if (!und) {
			pakname = GetLatestDpkPakVersion(pakname);
		}
		if (!pakname || g_loaded_dpk_paks.find(pakname) != g_loaded_dpk_paks.end()) {
			return;
		}

		PakfilePaths::iterator i = g_pakfile_paths.find(pakname);
		if (i == g_pakfile_paths.end()) {
			return;
		}

		if (i->second.is_pakfile) {
			arc = InitPakFile(FileSystemQ3API_getArchiveModules(), i->second.fullpath.c_str());
		} else {
			arc = AddDpkDir(i->second.fullpath.c_str());
		}
		g_loaded_dpk_paks.insert(pakname);

		depsFile = arc->openTextFile("DEPS");
	}

	if (!depsFile) {
		return;
	}

	{
		TextLinesInputStream<TextInputStream> istream = depsFile->getInputStream();

		CopiedString line;
		char *p_name;
		char *p_version;
		while (line = istream.readLine(), string_length(line.c_str())) {
			if (!DpkReadDepsLine(line.c_str(), &p_name, &p_version)) { continue; }
			if (!p_version) {
				const char *p_latest = GetLatestDpkPakVersion(p_name);
				if (p_latest) { LoadDpkPakWithDeps(p_latest); }
			} else {
				int len = string_length(p_name) + string_length(p_version) + 1;
				char *p_pakname = string_new(len);
				sprintf(p_pakname, "%s_%s", p_name, p_version);
				LoadDpkPakWithDeps(p_pakname);
				string_release(p_pakname, len);
			}
			string_release(p_name, string_length(p_name));
			if (p_version) { string_release(p_version, string_length(p_version)); }
		}
	}

	depsFile->release();
}

// end for Daemon DPK vfs

// =============================================================================
// Global functions

// reads all pak files from a dir
void InitDirectory( const char* directory, ArchiveModules& archiveModules ){
	int j;

	g_numForbiddenDirs = 0;
	StringTokeniser st( GlobalRadiant().getGameDescriptionKeyValue( "forbidden_paths" ), " " );
	for ( j = 0; j < VFS_MAXDIRS; ++j )
	{
		const char *t = st.getToken();
		if ( string_empty( t ) ) {
			break;
		}
		strncpy( g_strForbiddenDirs[g_numForbiddenDirs], t, PATH_MAX );
		g_strForbiddenDirs[g_numForbiddenDirs][PATH_MAX] = '\0';
		++g_numForbiddenDirs;
	}

	for ( j = 0; j < g_numForbiddenDirs; ++j )
	{
		char* dbuf = g_strdup( directory );
		if ( *dbuf && dbuf[strlen( dbuf ) - 1] == '/' ) {
			dbuf[strlen( dbuf ) - 1] = 0;
		}
		const char *p = strrchr( dbuf, '/' );
		p = ( p ? ( p + 1 ) : dbuf );
		if ( matchpattern( p, g_strForbiddenDirs[j], TRUE ) ) {
			g_free( dbuf );
			break;
		}
		g_free( dbuf );
	}
	if ( j < g_numForbiddenDirs ) {
		printf( "Directory %s matched by forbidden dirs, removed\n", directory );
		return;
	}

	if ( g_numDirs == VFS_MAXDIRS ) {
		return;
	}

	strncpy( g_strDirs[g_numDirs], directory, PATH_MAX );
	g_strDirs[g_numDirs][PATH_MAX] = '\0';
	FixDOSName( g_strDirs[g_numDirs] );
	AddSlash( g_strDirs[g_numDirs] );

	const char* path = g_strDirs[g_numDirs];

	g_numDirs++;

	{
		g_archives.push_back( archive_entry_t{ path, OpenArchive( path ), false } );
	}

	if ( g_bUsePak ) {
		GDir* dir = g_dir_open( path, 0, 0 );

		if ( dir != 0 ) {
			globalOutputStream() << "vfs directory: " << path << "\n";

			const char* ignore_prefix = "";
			const char* override_prefix = "";

			{
				// See if we are in "sp" or "mp" mapping mode
				const char* gamemode = gamemode_get();

				if ( strcmp( gamemode, "sp" ) == 0 ) {
					ignore_prefix = "mp_";
					override_prefix = "sp_";
				}
				else if ( strcmp( gamemode, "mp" ) == 0 ) {
					ignore_prefix = "sp_";
					override_prefix = "mp_";
				}
			}

			Archives archives;
			Archives archivesOverride;
			for (;; )
			{
				const char* name = g_dir_read_name( dir );
				if ( name == 0 ) {
					break;
				}

				for ( j = 0; j < g_numForbiddenDirs; ++j )
				{
					const char *p = strrchr( name, '/' );
					p = ( p ? ( p + 1 ) : name );
					if ( matchpattern( p, g_strForbiddenDirs[j], TRUE ) ) {
						break;
					}
				}
				if ( j < g_numForbiddenDirs ) {
					continue;
				}

				const char *ext = strrchr( name, '.' );

				if ( ext && !string_compare_nocase_upper( ext, ".pk3dir" ) ) {
					if ( g_numDirs == VFS_MAXDIRS ) {
						continue;
					}
					snprintf( g_strDirs[g_numDirs], PATH_MAX, "%s%s/", path, name );
					g_strDirs[g_numDirs][PATH_MAX] = '\0';
					FixDOSName( g_strDirs[g_numDirs] );
					AddSlash( g_strDirs[g_numDirs] );
					g_numDirs++;

					{
						g_archives.push_back( archive_entry_t{ g_strDirs[g_numDirs - 1], OpenArchive( g_strDirs[g_numDirs - 1] ), false } );
					}
				}

				if ( ( ext == 0 ) || *( ++ext ) == '\0' || GetArchiveTable( archiveModules, ext ) == 0 ) {
					continue;
				}

				// using the same kludge as in engine to ensure consistency
				if ( !string_empty( ignore_prefix ) && strncmp( name, ignore_prefix, strlen( ignore_prefix ) ) == 0 ) {
					continue;
				}
				if ( !string_empty( override_prefix ) && strncmp( name, override_prefix, strlen( override_prefix ) ) == 0 ) {
					archivesOverride.insert( name );
					continue;
				}

				archives.insert( name );
			}

			g_dir_close( dir );

			// add the entries to the vfs
			for ( Archives::iterator i = archivesOverride.begin(); i != archivesOverride.end(); ++i )
			{
				char filename[PATH_MAX];
				strcpy( filename, path );
				strcat( filename, ( *i ).c_str() );
				InitPakFile( archiveModules, filename );
			}
			for ( Archives::iterator i = archives.begin(); i != archives.end(); ++i )
			{
				char filename[PATH_MAX];
				strcpy( filename, path );
				strcat( filename, ( *i ).c_str() );
				InitPakFile( archiveModules, filename );
			}
		}
		else
		{
			globalErrorStream() << "vfs directory not found: " << path << "\n";
		}
	}
}

// frees all memory that we allocated
// FIXME TTimo this should be improved so that we can shutdown and restart the VFS without exiting Radiant?
//   (for instance when modifying the project settings)
void Shutdown()
{
	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		(*i).archive->release();
	}
	g_archives.clear();

	g_numDirs = 0;
	g_numForbiddenDirs = 0;

	g_pakfile_paths.clear();
	g_loaded_dpk_paks.clear();
}

const int VFS_SEARCH_PAK = 0x1;
const int VFS_SEARCH_DIR = 0x2;

int GetFileCount(const char *filename, int flag)
{
	int count = 0;
	char fixed[PATH_MAX + 1];

	strncpy(fixed, filename, PATH_MAX);
	fixed[PATH_MAX] = '\0';
	FixDOSName(fixed);

	if (!flag) {
		flag = VFS_SEARCH_PAK | VFS_SEARCH_DIR;
	}

	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (((*i).is_pakfile && (flag & VFS_SEARCH_PAK) != 0)
		    || (!(*i).is_pakfile && (flag & VFS_SEARCH_DIR) != 0)) {
			if ((*i).archive->containsFile(fixed)) {
				++count;
			}
		}
	}

	return count;
}

ArchiveFile *OpenFile(const char *filename)
{
	ASSERT_MESSAGE(strchr(filename, '\\') == 0, "path contains invalid separator '\\': \"" << filename << "\"");
	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		ArchiveFile *file = (*i).archive->openFile(filename);
		if (file != 0) {
			return file;
		}
	}

	return 0;
}

ArchiveTextFile *OpenTextFile(const char *filename)
{
	ASSERT_MESSAGE(strchr(filename, '\\') == 0, "path contains invalid separator '\\': \"" << filename << "\"");
	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		ArchiveTextFile *file = (*i).archive->openTextFile(filename);
		if (file != 0) {
			return file;
		}
	}

	return 0;
}

// NOTE: when loading a file, you have to allocate one extra byte and set it to \0
std::size_t LoadFile(const char *filename, void **bufferptr, int index)
{
	char fixed[PATH_MAX + 1];

	strncpy(fixed, filename, PATH_MAX);
	fixed[PATH_MAX] = '\0';
	FixDOSName(fixed);

	ArchiveFile *file = OpenFile(fixed);

	if (file != 0) {
		*bufferptr = malloc(file->size() + 1);
		// we need to end the buffer with a 0
		((char *) (*bufferptr))[file->size()] = 0;

		std::size_t length = file->getInputStream().read((InputStream::byte_type *) *bufferptr, file->size());
		file->release();
		return length;
	}

	*bufferptr = 0;
	return 0;
}

void FreeFile(void *p)
{
	free(p);
}

GSList *GetFileList(const char *dir, const char *ext, std::size_t depth)
{
	return GetListInternal(dir, ext, false, depth);
}

GSList *GetDirList(const char *dir, std::size_t depth)
{
	return GetListInternal(dir, 0, true, depth);
}

void ClearFileDirList(GSList **lst)
{
	while (*lst) {
		g_free((*lst)->data);
		*lst = g_slist_remove(*lst, (*lst)->data);
	}
}

const char *FindFile(const char *relative)
{
	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if ((*i).archive->containsFile(relative)) {
			return (*i).name.c_str();
		}
	}

	return "";
}

const char *FindPath(const char *absolute)
{
	const char *best = "";
	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (string_length((*i).name.c_str()) > string_length(best)) {
			if (path_equal_n(absolute, (*i).name.c_str(), string_length((*i).name.c_str()))) {
				best = (*i).name.c_str();
			}
		}
	}

	return best;
}


class Quake3FileSystem : public VirtualFileSystem {
public:
void initDirectory(const char *path)
{
	InitDirectory(path, FileSystemQ3API_getArchiveModules());
}

void initialise()
{
	load();
	globalOutputStream() << "filesystem initialised\n";
	g_observers.realise();
}

void load()
{
	ArchiveModules &archiveModules = FileSystemQ3API_getArchiveModules();
	bool is_dpk_vfs = 1;

	if (is_dpk_vfs) {
		const char *pakname;
		g_loaded_dpk_paks.clear();

		// Load DEPS from game pack
		LoadDpkPakWithDeps(NULL);

		// prevent VFS double start, for MapName="" and MapName="unnamed.map"
		if (string_length(GlobalRadiant().getMapName())) {
			// load map's paks from DEPS
			char *mappakname = GetCurrentMapDpkPakName();
			if (mappakname != NULL) {
				LoadDpkPakWithDeps(mappakname);
				string_release(mappakname, string_length(mappakname));
			}
		}

		g_pakfile_paths.clear();
		g_loaded_dpk_paks.clear();
	}
}

void clear()
{
	// like shutdown() but does not unrealise (keep map etc.)
	Shutdown();
}

void refresh()
{
	// like initialise() but does not realise (keep map etc.)
	load();
	globalOutputStream() << "filesystem refreshed\n";
}

void shutdown()
{
	g_observers.unrealise();
	globalOutputStream() << "filesystem shutdown\n";
	Shutdown();
}

int getFileCount(const char *filename, int flags)
{
	return GetFileCount(filename, flags);
}

ArchiveFile *openFile(const char *filename)
{
	return OpenFile(filename);
}

ArchiveTextFile *openTextFile(const char *filename)
{
	return OpenTextFile(filename);
}

std::size_t loadFile(const char *filename, void **buffer)
{
	return LoadFile(filename, buffer, 0);
}

void freeFile(void *p)
{
	FreeFile(p);
}

void forEachDirectory(const char *basedir, const FileNameCallback &callback, std::size_t depth)
{
	GSList *list = GetDirList(basedir, depth);

	for (GSList *i = list; i != 0; i = g_slist_next(i)) {
		callback(reinterpret_cast<const char *>((*i).data ));
	}

	ClearFileDirList(&list);
}

void forEachFile(const char *basedir, const char *extension, const FileNameCallback &callback, std::size_t depth)
{
	GSList *list = GetFileList(basedir, extension, depth);

	for (GSList *i = list; i != 0; i = g_slist_next(i)) {
		const char *name = reinterpret_cast<const char *>((*i).data );
		if (extension_equal(path_get_extension(name), extension)) {
			callback(name);
		}
	}

	ClearFileDirList(&list);
}

GSList *getDirList(const char *basedir)
{
	return GetDirList(basedir, 1);
}

GSList *getFileList(const char *basedir, const char *extension)
{
	return GetFileList(basedir, extension, 1);
}

void clearFileDirList(GSList **lst)
{
	ClearFileDirList(lst);
}

const char *findFile(const char *name)
{
	return FindFile(name);
}

const char *findRoot(const char *name)
{
	return FindPath(name);
}

void attach(ModuleObserver &observer)
{
	g_observers.attach(observer);
}

void detach(ModuleObserver &observer)
{
	g_observers.detach(observer);
}

Archive *getArchive(const char *archiveName, bool pakonly)
{
	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (pakonly && !(*i).is_pakfile) {
			continue;
		}
		if (path_equal((*i).name.c_str(), archiveName)) {
			return (*i).archive;
		}
	}
	return 0;
}

void forEachArchive(const ArchiveNameCallback &callback, bool pakonly, bool reverse)
{
	if (reverse) {
		g_archives.reverse();
	}

	for (archives_t::iterator i = g_archives.begin(); i != g_archives.end(); ++i) {
		if (pakonly && !(*i).is_pakfile) {
			continue;
		}

		callback((*i).name.c_str());
	}

	if (reverse) {
		g_archives.reverse();
	}
}
};


Quake3FileSystem g_Quake3FileSystem;

VirtualFileSystem &GetFileSystem()
{
	return g_Quake3FileSystem;
}

void FileSystem_Init()
{
}

void FileSystem_Shutdown()
{
}
