// Root-owned, group-executable launcher with cap_perfmon=ep. Keep the test and
// its children at the caller's UID while carrying only CAP_PERFMON across exec.
#include <linux/capability.h>
#include <stdio.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc < 2 || argv[1][0] != '/' || getuid() == 0 || getuid() != geteuid()) {
    fputs("run as a non-root user with an absolute test command\n", stderr);
    return 1;
  }
  struct __user_cap_header_struct header = {_LINUX_CAPABILITY_VERSION_3, 0};
  struct __user_cap_data_struct current[_LINUX_CAPABILITY_U32S_3] = {0};
  if (syscall(SYS_capget, &header, current) != 0) {
    perror("capget");
    return 1;
  }
  const unsigned word = CAP_PERFMON / 32;
  const unsigned bit = 1U << (CAP_PERFMON % 32);
  if (!(current[word].permitted & bit)) {
    fputs("launcher requires the CAP_PERFMON file capability\n", stderr);
    return 1;
  }
  struct __user_cap_data_struct selected[_LINUX_CAPABILITY_U32S_3] = {0};
  selected[word].permitted = selected[word].effective = selected[word].inheritable = bit;
  if (syscall(SYS_capset, &header, selected) != 0 ||
      prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_PERFMON, 0, 0) != 0) {
    perror("enable CAP_PERFMON for test");
    return 1;
  }
  execv(argv[1], argv + 1);
  perror("exec test");
  return 1;
}
