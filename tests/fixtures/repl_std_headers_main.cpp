// One pass of repl_std_headers.hpp per "g" byte on stdin, so a test counts events exactly. The
// loop polls a non-blocking stdin so the tracer can attach while the program waits.
#include "repl_std_headers.hpp"

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/prctl.h>
#include <unistd.h>

#ifndef PR_SET_PTRACER
#define PR_SET_PTRACER 0x59616d61
#endif
#ifndef PR_SET_PTRACER_ANY
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

// The sum of every result a pass observes, so the compiler cannot discard a call.
static long run_once(int pass) {
  stdh::Record record;
  const auto people = stdh::make_people(3);
  long sum = static_cast<long>(record.label().size()) + record.count() +
             static_cast<long>(stdh::make_label(pass).size()) + stdh::sum_people(people) +
             stdh::make_array(pass)[0] + static_cast<long>(stdh::make_ordered(3).size()) +
             static_cast<long>(stdh::make_pairs(2).size());
  const auto gadget = stdh::make_gadget();
  sum += stdh::inspect(gadget);
  const std::uniform_real_distribution<double> range(2.0, 6.0);
  sum += static_cast<long>(stdh::midpoint(range));
  sum += stdh::make_longs(3).back() + static_cast<long>(stdh::make_sizes(2).back()) +
         static_cast<long>(stdh::view_size("four")) + stdh::make_delay(pass).count() +
         stdh::make_stamp(pass).time_since_epoch().count() +
         stdh::call_twice([](long value) { return value + 1; }, pass) +
         static_cast<long>(stdh::make_modes(2).size()) + *stdh::make_level(pass) +
         static_cast<long>(stdh::make_named(2).size()) + stdh::find_named(pass)->rank;
  const std::ratio<1, 200> ratio;
  std::unique_ptr<int[]> values(new int[3]{1, 2, 3});
  const std::array<int[2][3], 1> grid{{{{1, 2, 3}, {4, 5, 6}}}};
  const std::map<int, int (*)(int)> handlers{{1, [](int value) { return value * 2; }},
                                             {2, [](int value) { return value + 5; }}};
  return sum + stdh::ratio_den(ratio) + stdh::sum_array(values, 3) + stdh::grid_sum(grid) +
         stdh::apply_all(handlers, pass);
}

int main() {
  if (::prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY) != 0 ||
      ::fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK) != 0)
    return 2;
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::puts("READY");
  int pass = 0;
  for (;;) {
    char command = 0;
    const auto count = ::read(STDIN_FILENO, &command, 1);
    if (count == 0 || command == 'q')
      break;
    if (count < 0) {
      if (errno != EAGAIN && errno != EINTR)
        return 4;
      ::usleep(10000);
      continue;
    }
    if (command == 'g') {
      std::printf("PASS %d %ld\n", pass, run_once(pass));
      ++pass;
    }
  }
  std::puts("EXIT");
  return 0;
}
