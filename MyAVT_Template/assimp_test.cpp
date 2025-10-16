// assimp_test.cpp
#include <iostream>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>

int main() {
    Assimp::Importer importer;
    std::cout << "Assimp visible, sizeof importer = " << sizeof(importer) << "\n";
    return 0;
}
