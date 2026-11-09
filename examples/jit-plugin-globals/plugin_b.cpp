extern "C" {
unsigned plugin_counter = 0;

unsigned tick() { return plugin_counter += 10; }
}
