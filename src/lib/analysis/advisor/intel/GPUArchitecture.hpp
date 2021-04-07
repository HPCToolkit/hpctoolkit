class GPUArchitecture {
 public:
  enum Vendor { VENDOR_NVIDIA = 0, VENDOR_AMD = 1, VENDOR_UNKNOWN = 2 };

  GPUArchitecture(Vendor vendor) : _vendor(vendor) {}

  // instruction latency
  // memory instruction latency varies
  // <min, max>
  virtual std::pair<int, int> latency(const std::string &opcode) const = 0;

  // warp throughput, not block throughput
  virtual int issue(const std::string &opcode) const = 0;

  virtual int inst_size() const = 0;

  // number of sms per GPU
  virtual int sms() const = 0;

  // number of schedulers per sm
  virtual int schedulers() const = 0;

  // number of warps per sm
  virtual int warps() const = 0;

  // number of threads per warp
  virtual int warp_size() const = 0;

  virtual double frequency() const = 0;

  virtual ~GPUArchitecture() {}

 protected:
  Vendor _vendor;
};
