#include <iostream>
#include "Logger.h"
using namespace std;
using namespace log4cpp;

Logger::Logger(): category(Category::getInstance("category")) {
    PatternLayout* layout1 = new PatternLayout();
    layout1->setConversionPattern("%d [%p] %m%n");
    PatternLayout* layout2 = new PatternLayout();
    layout2->setConversionPattern("%d [%p] %m%n");
    PatternLayout* layout3 = new PatternLayout();
    layout3->setConversionPattern("%d [%p] %m%n");
    
    OstreamAppender* osAppender = new OstreamAppender("console", &std::cout);
    osAppender->setLayout(layout1);
    FileAppender* fileAppender = new FileAppender("file", "mylog.log");
    fileAppender->setLayout(layout2);
    RollingFileAppender* rfAppender = new RollingFileAppender("rollingfile","rollingfile.log",5*1024*1024,9);
    rfAppender->setLayout(layout3);

    category.setPriority(Priority::DEBUG);
    category.addAppender(osAppender);
    category.addAppender(fileAppender);
    category.addAppender(rfAppender);
}
Logger::~Logger(){
    category.shutdown();
}
