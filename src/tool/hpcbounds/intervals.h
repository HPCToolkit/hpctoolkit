#include <map>

class intervals {
private:
  std::map<void *, void *> mymap;
public:
  void insert(void *start, void *end); 
  std::pair<void *const, void *> *contains(void * i); 
  void dump(); 
};
