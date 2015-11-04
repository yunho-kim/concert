#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
using namespace std;
int main(int argc, char *argv[]){
    FILE *fp = fopen("test_main.c", "w");
    ifstream in("initializer.dat");
    string str;
    fprintf(fp, "int main(){\n");
    while(in >> str){
        fprintf(fp, "%s();\n", str.c_str());
    }
    fprintf(fp, "%s_driver();\n", argv[1]);
    fprintf(fp, "return 0;}\n");
    fclose(fp);
}
