#include <iostream>
#include <string>

#include <kontsuba/converter.h>

int main(int argc, char const *argv[])
{
    const std::string path = "../../test_models/shapenet/models/model_normalized.obj";

    Kontsuba::Converter(path, "test");

    return 0;
}
