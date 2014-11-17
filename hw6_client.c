#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#define SYSCALLNUM 378
#define MYASSERT(x) do {if(!(!!(x))){fprintf(stderr, "%d::Assertion " #x " failed\n", __LINE__);return 1;}} while(0)

int dothecall(const char *src, const char *dest) {
	return syscall(SYSCALLNUM, src, dest);
}

int test_nullsrc(void) {
	int rc = dothecall(NULL, "helloworld");
	MYASSERT(rc == -1);
	MYASSERT(errno == EFAULT);
	return 0;
}

int test_srcnoexist(void) {
	int rc = dothecall("NOWAY", "helloworld");
	MYASSERT(rc == -1);
	MYASSERT(errno == ENOENT);
	return 0;
}

int test_srcnotreg(void) {
	system("mkdir zomg");
	int rc = dothecall("zomg", "helloworld");
	system("rmdir zomg");
	MYASSERT(rc == -1);
	MYASSERT(errno == EPERM);
	return 0;
}

char zomgbuffer[500000];

int diff_file_to_buffer(const char *fn, const char *buf, int count) {
	FILE *fp = fopen(fn, "r");
	int c;
	int ctr = 0;
	if(!fp) {
		return 0;
	}
	while((c = fgetc(fp)) != EOF) {
		if(ctr >= count) {
			printf("File too long\n");
			fclose(fp);
			return 0;
		}
		if(c != buf[ctr]) {
			fclose(fp);
			printf("Diff at char %d: %d versus %d\n", ctr, c, buf[ctr]);
			return 0;
		}
		++ctr;
	}
	fclose(fp);
	if(ctr != count){
		printf("Diff ct %d %d\n", ctr, count);
		return 0;
	}
	return 1;
}

int basic_ok(void) {
	int rc;
	FILE *fp = fopen("fexists", "w");
	int qq;
	char *pc = zomgbuffer;
	struct stat sa,sb;
	MYASSERT(fp);
	for(qq=0;qq<10000;++qq)
	{
		pc += sprintf(pc, "%d\n", qq);
	}
	MYASSERT(1 == fwrite(zomgbuffer, pc - zomgbuffer, 1, fp));
	fclose(fp);
	rc = dothecall("fexists", "goodhelloworld");
	MYASSERT(rc == 0);
	MYASSERT(0 == stat("fexists", &sa));
	MYASSERT(0 == stat("goodhelloworld", &sb));
	MYASSERT(sa.st_ino == sb.st_ino);

	fp = fopen("fexists", "a");
	MYASSERT(fp);
	fprintf(fp, "ZZOMG\n");
	sprintf(pc, "ZZOMG\n");
	fclose(fp);
	MYASSERT(0 == stat("fexists", &sa));
	MYASSERT(0 == stat("goodhelloworld", &sb));
	MYASSERT(sa.st_ino != sb.st_ino);
	/* open the two files and make sure they're ok */
	MYASSERT(diff_file_to_buffer("fexists", zomgbuffer, (pc - zomgbuffer) + 6));
	MYASSERT(diff_file_to_buffer("goodhelloworld", zomgbuffer, (pc - zomgbuffer)));
	system("rm -f fexists");
	system("rm -f goodhelloworld");
	return 0;
}

int basic_ok3(void) {
	int rc;
	FILE *fp = fopen("TA", "w");
	int qq;
	char *pc = zomgbuffer;
	struct stat sa,sb,sc;
	MYASSERT(fp);
	for(qq=0;qq<10000;++qq)
	{
		pc += sprintf(pc, "%d\n", qq);
	}
	MYASSERT(1 == fwrite(zomgbuffer, pc - zomgbuffer, 1, fp));
	fclose(fp);
	rc = dothecall("TA", "TB");
	MYASSERT(rc == 0);
	rc = dothecall("TB", "TC");
	MYASSERT(rc == 0);
	MYASSERT(0 == stat("TA", &sa));
	MYASSERT(0 == stat("TB", &sb));
	MYASSERT(0 == stat("TC", &sc));
	MYASSERT(sa.st_ino == sb.st_ino);
	MYASSERT(sb.st_ino == sc.st_ino);

	fp = fopen("TB", "a");
	MYASSERT(fp);
	fprintf(fp, "ZZOMG\n");
	sprintf(pc, "ZZOMG\n");
	fclose(fp);
	MYASSERT(0 == stat("TA", &sa));
	MYASSERT(0 == stat("TB", &sb));
	MYASSERT(0 == stat("TC", &sc));
	MYASSERT(sa.st_ino != sb.st_ino);
	MYASSERT(sc.st_ino != sb.st_ino);
	MYASSERT(sa.st_ino == sc.st_ino);

	/* are the files ok so far? */
	MYASSERT(diff_file_to_buffer("TB", zomgbuffer, (pc - zomgbuffer) + 6));
	MYASSERT(diff_file_to_buffer("TA", zomgbuffer, (pc - zomgbuffer)));
	MYASSERT(diff_file_to_buffer("TC", zomgbuffer, (pc - zomgbuffer)));

	/* modify file C */
	fp = fopen("TC", "a");
	MYASSERT(fp);
	fprintf(fp, "QQOMG\n");
	fclose(fp);

	MYASSERT(0 == stat("TA", &sa));
	MYASSERT(0 == stat("TB", &sb));
	MYASSERT(0 == stat("TC", &sc));
	MYASSERT(sa.st_ino != sb.st_ino);
	MYASSERT(sc.st_ino != sb.st_ino);
	MYASSERT(sa.st_ino != sc.st_ino);


	MYASSERT(diff_file_to_buffer("TA", zomgbuffer, (pc - zomgbuffer)));
	sprintf(pc, "ZZOMG\n");
	MYASSERT(diff_file_to_buffer("TB", zomgbuffer, (pc - zomgbuffer) + 6));
	sprintf(pc, "QQOMG\n");
	MYASSERT(diff_file_to_buffer("TC", zomgbuffer, (pc - zomgbuffer) + 6));

	system("rm -f fexists");
	system("rm -f goodhelloworld");
	return 0;
}

int src_notext4(void) {
	int rc;
	/* be root for this one */
	system("mkdir /dev/zomg");
	system("touch /dev/zomg/fexists");
	rc = dothecall("/dev/zomg/fexists", "/dev/zomg/helloworld");
	system("rm -rf /dev/zomg");
	MYASSERT(rc == -1);
	MYASSERT(errno = EOPNOTSUPP);
	return 0;
}

int src_isopen(void) {
	int rc;
	FILE *fp = fopen("opened_file", "w");
	MYASSERT(fp);
	rc = dothecall("opened_file", "helloworld");
	fclose(fp);
	system("rm -f opened_file");
	MYASSERT(rc == -1);
	MYASSERT(errno == EPERM);
	return 0;
}

int dest_isnull(void) {
	int rc;
	system("touch blah");
	rc = dothecall("blah", NULL);
	system("rm -f blah");
	MYASSERT(rc == -1);
	MYASSERT(errno == EFAULT);
	return 0;
}

int dest_exists(void) {
	int rc;
	system("touch blah");
	system("touch blah2");
	rc = dothecall("blah", "blah2");
	system("rm -f blah blah2");
	MYASSERT(rc == -1);
	MYASSERT(errno == EEXIST);
	return 0;
}

int dest_isdir(void) {
	int rc;
	system("touch blah");
	system("mkdir blah2");
	rc = dothecall("blah", "blah2");
	system("rm -rf blah blah2");
	MYASSERT(rc == -1);
	MYASSERT(errno == EEXIST);
	return 0;
} 

/* must be root for this one */
int not_samedev(void) {
	int rc;
	system("echo hello >blah");
	system("mkdir /dev/zomg");
	rc = dothecall("blah", "/dev/zomg/afile");
	system("rm -rf /dev/zomg");
	system("rm -f blah");
	MYASSERT(rc == -1);
	MYASSERT(errno == EXDEV);
	return 0;
}

typedef int (*utest)(void);

struct tester {
	const char *name;
	utest test_fn;
};
#define DEF_TEST(x) {#x, &x},

struct tester TESTS[] = {
DEF_TEST(test_nullsrc)
DEF_TEST(test_srcnoexist)
DEF_TEST(test_srcnotreg)
DEF_TEST(basic_ok)
DEF_TEST(basic_ok3)
DEF_TEST(src_notext4)
DEF_TEST(src_isopen)
DEF_TEST(dest_isnull)
DEF_TEST(dest_isdir)
DEF_TEST(not_samedev)
};

void run_test(int idx) {
	int TESTSZ = sizeof(TESTS)/sizeof(TESTS[0]);
	int rc;
	assert(idx >= 0 && idx < TESTSZ);
	printf("Running case %d: %s\n", idx, TESTS[idx].name);
	rc = (*TESTS[idx].test_fn)();
	if(rc != 0) {
		printf("Case #%d (%s): FAIL %d\n", idx,TESTS[idx].name, rc);
	}
	else {
		printf("Case #%d (%s): PASS\n", idx, TESTS[idx].name);
	}
}

int main(int argc, char **argv) {
	int arg;
	int TESTSZ = sizeof(TESTS)/sizeof(TESTS[0]);
	if(argc == 1) {
		fprintf(stderr, "Need argument number, -1 to run all\n");
		return 0;
	}
	arg = atoi(argv[1]);
	if(arg != -1 && !(arg >= 0 && arg < TESTSZ)) {
		fprintf(stderr, "Should be -1 or 0 - %d\n", TESTSZ);
		return 1;
	}
	if(arg == -1) {
		int i;
		for(i=0;i<TESTSZ;++i)
		{
			run_test(i);
		}
	}
	else {
		run_test(arg);
	}
	return 0;
}
