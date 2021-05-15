#include "statistics/main.h"
#include "initialize_classes.h"
#include "java/io/BufferedReader.class.h"
#include "java/io/InputStream.class.h"
#include "java/io/InputStreamReader.class.h"
#include "java/io/PrintStream.class.h"
#include "java/lang/Double.class.h"
#include "java/lang/String.class.h"
#include "java/lang/System.class.h"
#include "org/apache/commons/math3/stat/descriptive/SummaryStatistics.class.h"
#include "org/apache/commons/math3/stat/descriptive/StatisticalSummary.class.h"

#include <iostream>
#include <sstream>
#include <string>

using java::io::BufferedReader;
using java::io::InputStreamReader;
using java::lang::Double;
using java::lang::String;
using java::lang::System;
using org::apache::commons::math3::stat::descriptive::SummaryStatistics;
using org::apache::commons::math3::stat::descriptive::StatisticalSummary;

using namespace whatjni;

int main(int argc, const char **argv) {
    initialize_vm(vm_config(JNI_VERSION_1_8));
    initialize_classes();

    try {
        auto statistics = SummaryStatistics::new_object();

        System::out->println("Enter some numbers, one per line, then 'done' when finished:"_j);

        auto reader = BufferedReader::new_object(InputStreamReader::new_object(System::in));
        while (true) {
            auto line = reader->readLine();
            if (line == nullptr || line->equals("done"_j)) {
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
        System::out->println(j_string(stream.str()));

    } catch (const jvm_exception& e) {
        std::cout << "Exception " << e.get_message() << "\n";
    }
}
