#include "statistics/main.h"
#include "java/awt/Point.class.h"
#include "java/io/BufferedReader.class.h"
#include "java/io/InputStream.class.h"
#include "java/io/InputStreamReader.class.h"
#include "java/io/PrintStream.class.h"
#include "java/lang/Double.class.h"
#include "java/lang/String.class.h"
#include "java/lang/System.class.h"
#include "org/apache/commons/math3/stat/descriptive/SummaryStatistics.class.h"
#include "org/apache/commons/math3/stat/descriptive/StatisticalSummary.class.h"

#if false
#include "mymodule/NonExistent.class.h"
#endif

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using java::io::BufferedReader;
using java::io::InputStreamReader;
using java::lang::Double;
using java::lang::String;
using java::lang::System;
using org::apache::commons::math3::stat::descriptive::SummaryStatistics;
using org::apache::commons::math3::stat::descriptive::StatisticalSummary;

using namespace whatjni;

int main(int argc, const char **argv) {
    whatjni::load_vm_module(getenv("JVM_LIBRARY_PATH"));

    std::string defineClassPath = std::string("-Djava.class.path=") + getenv("CLASSPATH");

    std::vector<const char*> vm_args(argv + 1, argv + argc);
    vm_args.push_back(defineClassPath.data());

    whatjni::initialize_vm(JNI_VERSION_1_8, vm_args.size(), vm_args.data());

    try {
        auto statistics = SummaryStatistics::new_object();

        System::out->println("Enter some numbers, one per line, then 'done' when finished:"_j);


        auto reader = BufferedReader::new_object(InputStreamReader::new_object(System::in));
        while (true) {
            auto line = reader->readLine();
            if (line == nullptr || line->equals("done")) {
                break;
            }

            double number;
            try {
                number = Double::parseDouble(line);
            } catch (const jvm_exception& e) {
                System::err->println("Could not parse number"_j);
                continue;
            }

            statistics->addValue(number);
        }

        auto summary = statistics->getSummary();

        std::ostringstream stream;
        stream << "Average is " << summary->getMean() << "\n";
        stream << "Variance is " << summary->getVariance() << "\n";
        System::out->print(ref<String>(stream.str()));

    } catch (const jvm_exception& e) {
        std::cout << "Exception " << e.get_message() << "\n";
    }
}
