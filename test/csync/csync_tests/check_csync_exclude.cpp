/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "config_csync.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define CSYNC_TEST 1
#include "csync_exclude.cpp"

#include "torture.h"

#define EXCLUDE_LIST_FILE SOURCEDIR"/../../sync-exclude.lst"

static int setup(void **state) {
    CSYNC *csync;

    csync = new CSYNC("/tmp/check_csync1", new OCC::SyncJournalDb(""));

    *state = csync;
    return 0;
}

static int setup_init(void **state) {
    CSYNC *csync;
    int rc;

    csync = new CSYNC("/tmp/check_csync1", new OCC::SyncJournalDb(""));

    rc = csync_exclude_load(EXCLUDE_LIST_FILE, &(csync->excludes));
    assert_int_equal(rc, 0);

    /* and add some unicode stuff */
    rc = _csync_exclude_add(&(csync->excludes), "*.💩");
    assert_int_equal(rc, 0);
    rc = _csync_exclude_add(&(csync->excludes), "пятницы.*");
    assert_int_equal(rc, 0);
    rc = _csync_exclude_add(&(csync->excludes), "*/*.out");
    assert_int_equal(rc, 0);
    rc = _csync_exclude_add(&(csync->excludes), "latex*/*.run.xml");
    assert_int_equal(rc, 0);
    rc = _csync_exclude_add(&(csync->excludes), "latex/*/*.tex.tmp");
    assert_int_equal(rc, 0);

    csync_exclude_traversal_prepare(csync);

    *state = csync;
    return 0;
}

static int teardown(void **state) {
    CSYNC *csync = (CSYNC*)*state;
    int rc;

    auto statedb = csync->statedb;
    delete csync;
    delete statedb;

    rc = system("rm -rf /tmp/check_csync1");
    assert_int_equal(rc, 0);
    rc = system("rm -rf /tmp/check_csync2");
    assert_int_equal(rc, 0);

    *state = NULL;
    
    return 0;
}

static void check_csync_exclude_add(void **state)
{
  CSYNC *csync = (CSYNC*)*state;
  _csync_exclude_add(&(csync->excludes), "/tmp/check_csync1/*");
  assert_string_equal(csync->excludes->vector[0], "/tmp/check_csync1/*");
}

static void check_csync_exclude_load(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
    int rc;

    rc = csync_exclude_load(EXCLUDE_LIST_FILE, &(csync->excludes) );
    assert_int_equal(rc, 0);

    assert_string_equal(csync->excludes->vector[0], "*~");
    assert_int_not_equal(csync->excludes->count, 0);

    assert_true(csync->parsed_traversal_excludes.regexp_exclude.pattern().isEmpty());
    csync_exclude_traversal_prepare(csync); /* parse into regular expression */
    assert_false(csync->parsed_traversal_excludes.regexp_exclude.pattern().isEmpty());
}

static void check_csync_excluded(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
    int rc;

    rc = csync_excluded_no_ctx(csync->excludes, "", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, "/", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    rc = csync_excluded_no_ctx(csync->excludes, "A", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    rc = csync_excluded_no_ctx(csync->excludes, "krawel_krawel", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, ".kde/share/config/kwin.eventsrc", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, ".directory/cache-maximegalon/cache1.txt", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_no_ctx(csync->excludes, "mozilla/.directory", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /*
     * Test for patterns in subdirs. '.beagle' is defined as a pattern and has
     * to be found in top dir as well as in directories underneath.
     */
    rc = csync_excluded_no_ctx(csync->excludes, ".apdisk", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_no_ctx(csync->excludes, "foo/.apdisk", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_no_ctx(csync->excludes, "foo/bar/.apdisk", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_no_ctx(csync->excludes, ".java", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* Files in the ignored dir .java will also be ignored. */
    rc = csync_excluded_no_ctx(csync->excludes, ".apdisk/totally_amazing.jar", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /* and also in subdirs */
    rc = csync_excluded_no_ctx(csync->excludes, "projects/.apdisk/totally_amazing.jar", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /* csync-journal is ignored in general silently. */
    rc = csync_excluded_no_ctx(csync->excludes, ".csync_journal.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, ".csync_journal.db.ctmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, "subdir/.csync_journal.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    
    /* also the new form of the database name */
    rc = csync_excluded_no_ctx(csync->excludes, "._sync_5bdd60bdfcfa.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, "._sync_5bdd60bdfcfa.db.ctmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, "._sync_5bdd60bdfcfa.db-shm", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, "subdir/._sync_5bdd60bdfcfa.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);

    rc = csync_excluded_no_ctx(csync->excludes, ".sync_5bdd60bdfcfa.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, ".sync_5bdd60bdfcfa.db.ctmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, ".sync_5bdd60bdfcfa.db-shm", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, "subdir/.sync_5bdd60bdfcfa.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);


    /* pattern ]*.directory - ignore and remove */
    rc = csync_excluded_no_ctx(csync->excludes, "my.~directory", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_AND_REMOVE);

    rc = csync_excluded_no_ctx(csync->excludes, "/a_folder/my.~directory", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_AND_REMOVE);

    /* Not excluded because the pattern .netscape/cache requires directory. */
    rc = csync_excluded_no_ctx(csync->excludes, ".netscape/cache", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* Not excluded  */
    rc = csync_excluded_no_ctx(csync->excludes, "unicode/中文.hé", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    /* excluded  */
    rc = csync_excluded_no_ctx(csync->excludes, "unicode/пятницы.txt", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_no_ctx(csync->excludes, "unicode/中文.💩", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /* path wildcards */
    rc = csync_excluded_no_ctx(csync->excludes, "foobar/my_manuscript.out", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_no_ctx(csync->excludes, "latex_tmp/my_manuscript.run.xml", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_no_ctx(csync->excludes, "word_tmp/my_manuscript.run.xml", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    rc = csync_excluded_no_ctx(csync->excludes, "latex/my_manuscript.tex.tmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    rc = csync_excluded_no_ctx(csync->excludes, "latex/songbook/my_manuscript.tex.tmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

#ifdef _WIN32
    rc = csync_excluded_no_ctx(csync->excludes, "file_trailing_space ", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_TRAILING_SPACE);

    rc = csync_excluded_no_ctx(csync->excludes, "file_trailing_dot.", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_INVALID_CHAR);

    rc = csync_excluded_no_ctx(csync->excludes, "AUX", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_INVALID_CHAR);

    rc = csync_excluded_no_ctx(csync->excludes, "file_invalid_char<", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_INVALID_CHAR);
#endif

    /* ? character */
    _csync_exclude_add( &(csync->excludes), "bond00?" );
    csync_exclude_traversal_prepare(csync);
    rc = csync_excluded_no_ctx(csync->excludes, "bond00", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, "bond007", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_no_ctx(csync->excludes, "bond0071", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

#ifndef _WIN32
    /* brackets */
    _csync_exclude_add( &(csync->excludes), "a [bc] d" );
    csync_exclude_traversal_prepare(csync);
    rc = csync_excluded_no_ctx(csync->excludes, "a d d", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, "a  d", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, "a b d", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_no_ctx(csync->excludes, "a c d", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
#endif

    /* escapes */
    _csync_exclude_add( &(csync->excludes), "\\a \\* \\?" );
    csync_exclude_traversal_prepare(csync);
    rc = csync_excluded_no_ctx(csync->excludes, "\\a \\* \\?", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, "a b c", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_no_ctx(csync->excludes, "a * ?", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
}

static void check_csync_excluded_traversal(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
    int rc;

    rc = csync_excluded_traversal(csync, "", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_traversal(csync, "/", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    rc = csync_excluded_traversal(csync, "A", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    rc = csync_excluded_traversal(csync,  "krawel_krawel", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_traversal(csync,  ".kde/share/config/kwin.eventsrc", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_traversal(csync,  "mozilla/.directory", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /*
     * Test for patterns in subdirs. '.beagle' is defined as a pattern and has
     * to be found in top dir as well as in directories underneath.
     */
    rc = csync_excluded_traversal(csync,  ".apdisk", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_traversal(csync,  "foo/.apdisk", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_traversal(csync,  "foo/bar/.apdisk", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_traversal(csync, ".java", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* csync-journal is ignored in general silently. */
    rc = csync_excluded_traversal(csync, ".csync_journal.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_traversal(csync, ".csync_journal.db.ctmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_traversal(csync, "subdir/.csync_journal.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_traversal(csync, "/two/subdir/.csync_journal.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);

    /* also the new form of the database name */
    rc = csync_excluded_traversal(csync, "._sync_5bdd60bdfcfa.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_traversal(csync, "._sync_5bdd60bdfcfa.db.ctmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_traversal(csync, "._sync_5bdd60bdfcfa.db-shm", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_traversal(csync, "subdir/._sync_5bdd60bdfcfa.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);

    rc = csync_excluded_traversal(csync,  ".sync_5bdd60bdfcfa.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_traversal(csync,  ".sync_5bdd60bdfcfa.db.ctmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_traversal(csync,  ".sync_5bdd60bdfcfa.db-shm", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);
    rc = csync_excluded_traversal(csync,  "subdir/.sync_5bdd60bdfcfa.db", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_SILENTLY_EXCLUDED);


    /* pattern ]*.directory - ignore and remove */
    rc = csync_excluded_traversal(csync,  "my.~directory", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_AND_REMOVE);

    rc = csync_excluded_traversal(csync,  "/a_folder/my.~directory", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_AND_REMOVE);

    /* Not excluded because the pattern .netscape/cache requires directory. */
    rc = csync_excluded_traversal(csync,  ".netscape/cache", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* Not excluded  */
    rc = csync_excluded_traversal(csync,  "unicode/中文.hé", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    /* excluded  */
    rc = csync_excluded_traversal(csync,  "unicode/пятницы.txt", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_traversal(csync,  "unicode/中文.💩", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /* path wildcards */
    rc = csync_excluded_traversal(csync,  "foobar/my_manuscript.out", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_traversal(csync,  "latex_tmp/my_manuscript.run.xml", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_traversal(csync,  "word_tmp/my_manuscript.run.xml", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    rc = csync_excluded_traversal(csync,  "latex/my_manuscript.tex.tmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    rc = csync_excluded_traversal(csync,  "latex/songbook/my_manuscript.tex.tmp", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

#ifdef _WIN32
    rc = csync_excluded_traversal(csync,  "file_trailing_space ", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_TRAILING_SPACE);

    rc = csync_excluded_traversal(csync,  "file_trailing_dot.", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_INVALID_CHAR);

    rc = csync_excluded_traversal(csync,  "AUX", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_INVALID_CHAR);

    rc = csync_excluded_traversal(csync,  "file_invalid_char<", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_INVALID_CHAR);
#endif


    /* From here the actual traversal tests */

    _csync_exclude_add( &(csync->excludes), "/exclude" );
    csync_exclude_traversal_prepare(csync);

    /* Check toplevel dir, the pattern only works for toplevel dir. */
    rc = csync_excluded_traversal(csync, "/exclude", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_traversal(csync, "/foo/exclude", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* check for a file called exclude. Must still work */
    rc = csync_excluded_traversal(csync, "/exclude", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_traversal(csync, "/foo/exclude", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* Add an exclude for directories only: excl/ */
    _csync_exclude_add( &(csync->excludes), "excl/" );
    csync_exclude_traversal_prepare(csync);
    rc = csync_excluded_traversal(csync, "/excl", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_traversal(csync, "meep/excl", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_traversal(csync, "meep/excl/file", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED); // because leading dirs aren't checked!

    rc = csync_excluded_traversal(csync, "/excl", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    _csync_exclude_add(&csync->excludes, "/excludepath/withsubdir");
    csync_exclude_traversal_prepare(csync);

    rc = csync_excluded_traversal(csync, "/excludepath/withsubdir", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_traversal(csync, "/excludepath/withsubdir", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_traversal(csync, "/excludepath/withsubdir2", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    rc = csync_excluded_traversal(csync, "/excludepath/withsubdir/foo", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED); // because leading dirs aren't checked!

    /* Check ending of pattern */
    rc = csync_excluded_traversal(csync, "/exclude", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_traversal(csync, "/excludeX", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_traversal(csync, "exclude", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    _csync_exclude_add( &(csync->excludes), "exclude" );
    csync_exclude_traversal_prepare(csync);
    rc = csync_excluded_traversal(csync, "exclude", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    /* ? character */
    _csync_exclude_add( &(csync->excludes), "bond00?" );
    csync_exclude_traversal_prepare(csync);
    rc = csync_excluded_traversal(csync, "bond00", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_traversal(csync, "bond007", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_traversal(csync, "bond0071", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

#ifndef _WIN32
    /* brackets */
    _csync_exclude_add( &(csync->excludes), "a [bc] d" );
    csync_exclude_traversal_prepare(csync);
    rc = csync_excluded_traversal(csync, "a d d", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_traversal(csync, "a  d", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_traversal(csync, "a b d", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
    rc = csync_excluded_traversal(csync, "a c d", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
#endif

    /* escapes */
    _csync_exclude_add( &(csync->excludes), "\\a \\* \\?" );
    csync_exclude_traversal_prepare(csync);
    rc = csync_excluded_traversal(csync, "\\a \\* \\?", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_traversal(csync, "a b c", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);
    rc = csync_excluded_traversal(csync, "a * ?", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
}

static void check_csync_pathes(void **state)
{
    CSYNC *csync = (CSYNC*)*state;
    int rc;

    _csync_exclude_add( &(csync->excludes), "/exclude" );

    /* Check toplevel dir, the pattern only works for toplevel dir. */
    rc = csync_excluded_no_ctx(csync->excludes, "/exclude", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_no_ctx(csync->excludes, "/foo/exclude", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* check for a file called exclude. Must still work */
    rc = csync_excluded_no_ctx(csync->excludes, "/exclude", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_no_ctx(csync->excludes, "/foo/exclude", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    /* Add an exclude for directories only: excl/ */
    _csync_exclude_add( &(csync->excludes), "excl/" );
    rc = csync_excluded_no_ctx(csync->excludes, "/excl", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_no_ctx(csync->excludes, "meep/excl", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_no_ctx(csync->excludes, "meep/excl/file", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_no_ctx(csync->excludes, "/excl", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    _csync_exclude_add(&csync->excludes, "/excludepath/withsubdir");

    rc = csync_excluded_no_ctx(csync->excludes, "/excludepath/withsubdir", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_no_ctx(csync->excludes, "/excludepath/withsubdir", CSYNC_FTW_TYPE_FILE);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);

    rc = csync_excluded_no_ctx(csync->excludes, "/excludepath/withsubdir2", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_NOT_EXCLUDED);

    rc = csync_excluded_no_ctx(csync->excludes, "/excludepath/withsubdir/foo", CSYNC_FTW_TYPE_DIR);
    assert_int_equal(rc, CSYNC_FILE_EXCLUDE_LIST);
}

static void check_csync_is_windows_reserved_word(void **) {
    assert_true(csync_is_windows_reserved_word("CON"));
    assert_true(csync_is_windows_reserved_word("con"));
    assert_true(csync_is_windows_reserved_word("CON."));
    assert_true(csync_is_windows_reserved_word("con."));
    assert_true(csync_is_windows_reserved_word("CON.ference"));
    assert_false(csync_is_windows_reserved_word("CONference"));
    assert_false(csync_is_windows_reserved_word("conference"));
    assert_false(csync_is_windows_reserved_word("conf.erence"));
    assert_false(csync_is_windows_reserved_word("co"));

    assert_true(csync_is_windows_reserved_word("COM2"));
    assert_true(csync_is_windows_reserved_word("com2"));
    assert_true(csync_is_windows_reserved_word("COM2."));
    assert_true(csync_is_windows_reserved_word("com2."));
    assert_true(csync_is_windows_reserved_word("COM2.ference"));
    assert_false(csync_is_windows_reserved_word("COM2ference"));
    assert_false(csync_is_windows_reserved_word("com2ference"));
    assert_false(csync_is_windows_reserved_word("com2f.erence"));
    assert_false(csync_is_windows_reserved_word("com"));

    assert_true(csync_is_windows_reserved_word("CLOCK$"));
    assert_true(csync_is_windows_reserved_word("$Recycle.Bin"));
    assert_true(csync_is_windows_reserved_word("ClocK$"));
    assert_true(csync_is_windows_reserved_word("$recycle.bin"));

    assert_true(csync_is_windows_reserved_word("A:"));
    assert_true(csync_is_windows_reserved_word("a:"));
    assert_true(csync_is_windows_reserved_word("z:"));
    assert_true(csync_is_windows_reserved_word("Z:"));
    assert_true(csync_is_windows_reserved_word("M:"));
    assert_true(csync_is_windows_reserved_word("m:"));
}

/* QT_ENABLE_REGEXP_JIT=0 to get slower results :-) */
static void check_csync_excluded_performance(void **state)
{
    CSYNC *csync = (CSYNC*)*state;

    const int N = 10000;
    int totalRc = 0;
    int i = 0;

    // Being able to use QElapsedTimer for measurement would be nice...
    {
        struct timeval before, after;
        gettimeofday(&before, 0);

        for (i = 0; i < N; ++i) {
            totalRc += csync_excluded_no_ctx(csync->excludes, "/this/is/quite/a/long/path/with/many/components", CSYNC_FTW_TYPE_DIR);
            totalRc += csync_excluded_no_ctx(csync->excludes, "/1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16/17/18/19/20/21/22/23/24/25/26/27/29", CSYNC_FTW_TYPE_FILE);
        }
        assert_int_equal(totalRc, CSYNC_NOT_EXCLUDED); // mainly to avoid optimization

        gettimeofday(&after, 0);

        const double total = (after.tv_sec - before.tv_sec)
                + (after.tv_usec - before.tv_usec) / 1.0e6;
        const double perCallMs = total / 2 / N * 1000;
        printf("csync_excluded: %f ms per call\n", perCallMs);
    }

    {
        struct timeval before, after;
        gettimeofday(&before, 0);

        for (i = 0; i < N; ++i) {
            totalRc += csync_excluded_traversal(csync, "/this/is/quite/a/long/path/with/many/components", CSYNC_FTW_TYPE_DIR);
            totalRc += csync_excluded_traversal(csync, "/1/2/3/4/5/6/7/8/9/10/11/12/13/14/15/16/17/18/19/20/21/22/23/24/25/26/27/29", CSYNC_FTW_TYPE_FILE);
        }
        assert_int_equal(totalRc, CSYNC_NOT_EXCLUDED); // mainly to avoid optimization

        gettimeofday(&after, 0);

        const double total = (after.tv_sec - before.tv_sec)
                + (after.tv_usec - before.tv_usec) / 1.0e6;
        const double perCallMs = total / 2 / N * 1000;
        printf("csync_excluded_traversal: %f ms per call\n", perCallMs);
    }
}

static void check_csync_exclude_expand_escapes(void **state)
{
    (void)state;

    const char *str = csync_exclude_expand_escapes(
            "keep \\' \\\" \\? \\\\ \\a \\b \\f \\n \\r \\t \\v \\z \\#");
    assert_true(0 == strcmp(
            str, "keep ' \" ? \\ \a \b \f \n \r \t \v \\z #"));
    SAFE_FREE(str);

    str = csync_exclude_expand_escapes("");
    assert_true(0 == strcmp(str, ""));
    SAFE_FREE(str);

    str = csync_exclude_expand_escapes("\\");
    assert_true(0 == strcmp(str, "\\"));
    SAFE_FREE(str);
}

int torture_run_tests(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(check_csync_exclude_add, setup, teardown),
        cmocka_unit_test_setup_teardown(check_csync_exclude_load, setup, teardown),
        cmocka_unit_test_setup_teardown(check_csync_excluded, setup_init, teardown),
        cmocka_unit_test_setup_teardown(check_csync_excluded_traversal, setup_init, teardown),
        cmocka_unit_test_setup_teardown(check_csync_pathes, setup_init, teardown),
        cmocka_unit_test_setup_teardown(check_csync_is_windows_reserved_word, setup_init, teardown),
        cmocka_unit_test_setup_teardown(check_csync_excluded_performance, setup_init, teardown),
        cmocka_unit_test(check_csync_exclude_expand_escapes),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
