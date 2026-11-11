#include "item.hpp"

#include <string>

#ifdef HANDLE_WIDGET
class HFW::Widget::Impl {
public:
  int secret = 5;
  std::string name = "private implementation";
};

class HFW::Gadget::Impl {
public:
  int secret = 6;
  std::string name = "private referenced implementation";
};

class HFW::PolyWidget::Impl {
public:
  virtual ~Impl() = default;
  int secret = 7;
};

class HFW::PolyWidget::Special : public Impl {
public:
  int extra = 8;
};

HFW::PolyWidget::PolyWidget() : id(5), impl_(new Special), plain_(new Impl) {}
HFW::PolyWidget::~PolyWidget() {
  delete impl_;
  delete plain_;
}

HFW::Widget::Widget() : id(3), impl_(new Impl), owned_(new Impl) {}
HFW::Widget::~Widget() { delete impl_; }
HFW::Gadget::Gadget() : id(4), impl_(*new Impl) {}
HFW::Gadget::~Gadget() { delete &impl_; }
#endif
