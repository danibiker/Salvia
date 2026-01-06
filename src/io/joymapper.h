#include <const/Constant.h>
#include <beans/structures.h>
#include <iterator>
#include <string>

static const int defaultButtons[] = {1,2,-1,-1,4,5,8,-1,-1,-1,101,-1,108,-1,104,-1,102,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};

class JoyMapper
{
    protected:
    private:
        static int *joyButtonMapper;
        static std::string rutaIni;
    public:
        JoyMapper();
        virtual ~JoyMapper();
        static bool initJoyMapper();
        static int getJoyMapper(int button);
        static void setJoyMapper(int button, int value);
        static void clearJoyMapper();
        static void saveJoyConfig();
};

