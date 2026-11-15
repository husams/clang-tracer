// The state every probe of this lesson shares. Each hook file includes it.
// The callback compiler supplies Call, State and shared_state.
#include <string>
#include <vector>

struct RequestScope {};

// One traced call made while serving a request.
struct Step {
  std::string function;
  int argument = 0; // sku for price_item, otherwise the user
  int result = 0;
};

// One object exists per request. The engine creates it when the outermost
// probed call starts and releases it when that call returns, so overlapping
// requests on different threads never see each other's steps.
struct RequestState : shared_state<RequestScope> {
  int request = 0;
  std::vector<Step> steps;
};
