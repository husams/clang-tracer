// Parameter names come from DWARF and values are carried in Protobuf.
class Interceptor {
public:
  void pre(const Call &call) {
    call.argument<int>(0);
    call.argument<int>(1);
  }

  void post(const Call &call) {
    call.argument<int>(0);
    call.argument<int>(1);
  }
};
