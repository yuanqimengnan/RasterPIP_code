#include <QApplication>
#include <QDebug>
#include <QString>
#include <QFile>

#include "GLHandler.hpp"
#include "DataHandler.hpp"
#include "UsefulFuncs.hpp"
#include <QElapsedTimer>
#include <string.h>
#include <forexperiment.h>
#include <iomanip>


//#define BASELINE
#ifdef BASELINE
#include "baseline.hpp"
#endif

using namespace std;

/*Global variables for the command line options */
size_t nIter, location_attribute, nAttrib;
int accuracy, gridRes;
QString joinType, databaseName, polyFile, polyData, experiment;
uint32_t start_time, end_time;

//Handlers
GLHandler* handler;
DataHandler* dataHandler;
set<size_t> attributes;
uint64_t inputSize;
float* points;
bool inMemory;
bool opAgg, opTime;
std::vector<QueryConstraint> constraints;
int aggrAttrib;
QString timeFile, aggFile;

int64_t gpuMemInMB;

//output
QVector<int> agg, bounds;

//for experiment output
float_t experiment_time[10] = {0,0,0,0,0,0,0,0,0,0};
string title = "Query Index,triangle,pass polygons,pass points,render Polygons,render Points,get result,time,Coarse Query points number,result size";
//choose dataset;see forexperiment.h and UsefulFuncs.cpp
int dataset = 1;
// set batch size
int single_pass_size = 32;
int batch_size = single_pass_size * 4;

void ensureOpenGLFormat()
{
    QSurfaceFormat glf = QSurfaceFormat::defaultFormat();
#if defined (__APPLE__)
    glf.setVersion(4,1);
    glf.setProfile(QSurfaceFormat::CoreProfile);
#else
    glf.setVersion(4,5);
    glf.setProfile(QSurfaceFormat::CompatibilityProfile);
#endif
    glf.setSamples(4);
    glf.setSwapBehavior(QSurfaceFormat::SingleBuffer);
    glf.setRedBufferSize(8);
    glf.setGreenBufferSize(8);
    glf.setBlueBufferSize(8);
    glf.setDepthBufferSize(8);

    //qDebug() << glf.version();
    QSurfaceFormat::setDefaultFormat(glf);
}

GLHandler::FunctionType getJoinOperator() {
    GLHandler::FunctionType joinOperator;
    if (joinType == "raster") {
        joinOperator = GLHandler::RasterJoinFn;
    }
    else if(joinType == "hybrid") {
        // hybrid raster join - accurate
        joinOperator = GLHandler::HybridJoinFn;
    } else if(joinType == "index") {
        // brute force join - accurate
        joinOperator = GLHandler::IndexJoinFn;
    } else if(joinType == "errorbounds") {
        // compute error bounds of raster approach
        joinOperator = GLHandler::RasterJoinBoundFn;
    } else {
        cerr << "Wrong join type " << endl;
        exit(0);
    }
    return joinOperator;
}

void setupConstraints() {
    // Hardcoding attribute constraints, works only for taxi
    constraints.clear();
    if (nAttrib >= 1) {
        QueryConstraint t;
        t.attribId = 18; //passengers
        t.type = EQ;
        t.val.uval = 1;
        constraints.push_back(t);
    }
    if (nAttrib >= 2) {
        QueryConstraint t;
        t.attribId = 8; //fare
        t.type = GT;
        t.val.uval = 1000;
        constraints.push_back(t);
    }
    if (nAttrib >= 3) {
        QueryConstraint t;
        t.attribId = 7; //miles
        t.type = LT;
        t.val.fval = 10;
        constraints.push_back(t);
    }
    if (nAttrib == 4) {
        QueryConstraint t;
        t.attribId = 6; //duration
        t.type = GT;
        t.val.uval = 20;
        constraints.push_back(t);
    }

    for(size_t i = 0;i < constraints.size();i ++) {
        attributes.insert(constraints[i].attribId);
//        qDebug() << "Attributes " << constraints[i].attribId << " ";
    }
}

void setupExperiment() {
    bool useIndexBackend = true;
    if(useIndexBackend) {
        // init data handler
        dataHandler = new DataHandler();
        dataHandler->initData(polyFile, databaseName, attributes, location_attribute);

        dataHandler->setPolygonQuery(polyData);
        Bound bound = dataHandler->getPolyHandler()->getBounds();

        // query index
        float region_low[3] = {(float)bound.leftBottom.x(), (float) bound.leftBottom.y(), (float) start_time}; //TODO: keep time as long in the index
        float region_high[3] = {(float) bound.rightTop.x(), (float) bound.rightTop.y(), (float) end_time};
        //qDebug() << "querying";
        dataHandler->executeQuery(region_low, region_high);
        //TODO: Delete the content-> add a method in Query Result so that the whole content is cleaned up.

        if(nAttrib > 0) {
            dataHandler->setQueryConstraints(constraints);
        }
        if(aggrAttrib != -1) {
            dataHandler->setAggregation(Avg,aggrAttrib);
        }
        QElapsedTimer t1;
        t1.start();
        handler = GLHandler::getInstance(gpuMemInMB, inMemory);
        std::cout<<"InitGL"<<t1.elapsed()<<"ms"<<endl;
        if(handler == NULL) {
            std::cout << "Failed to obtain handler";
            exit(0);
        }
        handler->setDataHandler(dataHandler);
        handler->setAccuracyDistance(accuracy);
        handler->setPolyIndexResolution(gridRes,gridRes);
    } else {
//        sortedDataHandler = new SortedDataHandler();
//        uint64_t numberOfBytes = inputSize*2*sizeof(float);
//        sortedDataHandler->initData(polyFile, databaseName, attributes, location_attribute);
//        sortedDataHandler->executeQuery(numberOfBytes);

//        handler = GLHandler::getInstance(inMemory);
//        if(handler == NULL) {
//            std::cout << "Failed to obtain handler";
//            exit(0);
//        }
//        handler->setDataHandler(sortedDataHandler);
//        handler->setAccuracyDistance(accuracy);
//        handler->setPolyIndexResolution(gridRes,gridRes);
    }
}

void runExperiment() {
    GLHandler::FunctionType joinType = getJoinOperator();
    for (size_t k=0; k<nIter; k++) {
       // qDebug() << "iteration" << k;
        agg = handler->executeFunction(joinType);
     //   bounds = handler->getErrorBounds(joinType);
    }
    // agg = handler->executeFunction(joinType);
    // bounds = handler->getErrorBounds(j);
}

void outputErrorBounds(uint32_t endTime = 0, int res = 0) {
    int np = bounds.size() / 4;
    QString fileName = aggFile + "/err_bound_" + QString::number(endTime) + "_" + QString::number(res) + ".csv";
    QFile f(fileName);
    if (!f.open(QFile::WriteOnly | QFile::Text)) return;
    QTextStream op(&f);
    for(int i = 0;i < np;i ++) {
        int fn = bounds[i];
        int fp = bounds[i + np];
        int fn1 = bounds[i + np * 2];
        int fp1 = bounds[i + np * 3];
        op << i << "\t" << (agg[i] - fp1) << "\t" << (agg[i] + fn1) << "\t"
                        << (agg[i] - fp) << "\t" << (agg[i] + fn) << "\n";
    }
    f.close();
}

void printResults(QVector<int> agg, int top, uint32_t endTime = 0, int res = 0) {
    int polySize = agg.size() / 3;
    if(endTime > 0) {
        QString fileName = aggFile + "/raster_" + QString::number(endTime) + "_" + QString::number(res) + ".csv";
        QFile f(fileName);
        if (!f.open(QFile::WriteOnly | QFile::Text)) return;
        QTextStream op(&f);
        for(int i = 0;i < top; i ++) {
            if(aggrAttrib == -1) { // count
                op << i << "\t" << agg[i] << "\n";
            } else { // avg
                float num = float(agg[polySize + i]) * 100.f + agg[2 * polySize + i] / 10.f;
                float den = agg[i];
                float avg = (agg[i] == 0)? 0: num / den;
                op << qSetRealNumberPrecision(10) << i << "\t" << avg << "\n";
            }
        }
    } else {
        for(int i = 0;i < top; i ++) {
            if(aggrAttrib == -1) { // count
                std::cout << i << "\t" << agg[i] << "\t" << std::endl;
            } else { // avg
                float num = float(agg[polySize + i]) * 100.f + agg[2 * polySize + i] / 10.f;
                float den = agg[i];
                float avg = (agg[i] == 0)? 0: num / den;
                std::cout << i << "\t" << avg << "\n";
//                std::cout << i << "\t" << agg[i] << " " << agg[polySize + i] << " " << agg[polySize * 2 + i] << "\n";
            }
        }
    }
}

void outputResults() {
    if(opAgg) {
        if(getJoinOperator() != GLHandler::RasterJoinFn && getJoinOperator() != GLHandler::RasterJoinBoundFn) {
            accuracy = 0;
        }
        if(getJoinOperator() == GLHandler::RasterJoinBoundFn) {
            outputErrorBounds(end_time,accuracy);
        } else {
            printResults(agg,agg.size()/3,end_time,accuracy);
        }
    }
    {
        GLHandler::FunctionType joinType = getJoinOperator();
        QString timing = handler->printTimeStats(joinType);
        if(opTime) {
            QFile fi(timeFile);
            if(!fi.open(QFile::Append | QFile::Text)) {
                std::cerr << "could not open file for writing: " << timeFile.toStdString() << "\n";
            }
            QTextStream op(&fi);
            op << timing << "\n";
            fi.close();
        }
//        printResults(agg,10);
    }
}

bool parseArguments(const QMap<QString,QString> &args) {
    QList<QString> keys = args.keys();
//    qDebug() << keys;
    if(keys.contains("--help")) {
        return false;
    }
    if(keys.contains("--nIter")) {
        nIter = args["--nIter"].toInt();
    } else {
        nIter = 1;
    }
    if(keys.contains("--accuracy")) {
        accuracy = args["--accuracy"].toInt();
    } else {
        accuracy = 100;
    }

    if(keys.contains("--joinType")) {
        joinType = args["--joinType"];
    } else {
        return false;
    }
    if(keys.contains("--backendIndexName")) {
        databaseName = args["--backendIndexName"];
    } else {
        return false;
    }
    if(keys.contains("--polygonList")) {
        polyFile = args["--polygonList"];
    } else {
        return false;
    }
    if(keys.contains("--polygonDataset")) {
        polyData = args["--polygonDataset"];
    } else {
        return false;
    }
    if(keys.contains("--locAttrib")) {
//        qDebug() << "found loc attrib";
        location_attribute = args["--locAttrib"].toInt();
    } else {
        return false;
    }
    if(keys.contains("--indexRes")) {
        gridRes = args["--indexRes"].toInt();
    } else {
        gridRes = 256;
    }
    if(keys.contains("--nAttrib")) {
        nAttrib = args["--nAttrib"].toInt();
    } else {
        nAttrib = 0;
    }
    if(keys.contains("--startTime")) {
        start_time = (uint32_t)args["--startTime"].toLongLong();
    } else {
        return false;
    }
    if(keys.contains("--endTime")) {
        end_time = (uint32_t)args["--endTime"].toLongLong();
    } else {
        return false;
    }
    if(keys.contains("--inmem")) {
        inMemory = true;
    } else {
        inMemory = false;
    }
    if(keys.contains("--opAggregation")) {
        aggFile = args["--opAggregation"];
        opAgg = true;
    } else {
        opAgg = false;
    }
    if(keys.contains("--inputSize")) {
        inputSize = (uint32_t)args["--inputSize"].toLongLong();
    } else {
        inputSize = 0;
    }
    if(keys.contains("--avg")) {
        aggrAttrib = (uint32_t)args["--avg"].toInt();
    } else {
        aggrAttrib = -1;
    }

    if(keys.contains("--outputTime")) {
        opTime = true;
        timeFile = args["--outputTime"].trimmed();
    } else {
        opTime = false;
    }
    if(keys.contains("--gpuMem")) {
        gpuMemInMB = (int64_t)args["--gpuMem"].toLongLong();
    } else {
        // default is 3GB = 3072MB
        gpuMemInMB = 8LL * 256LL;
    }
    return true;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    std::cout<<"This is begin"<<std::endl;

#ifdef BASELINE
    uint32_t start_time = 1230768000;
    uint32_t end_time[] = {1293838000,1283328000,1272808000,1262298000,1251178000,1241268000};
    int ct = 6;
    for(int i = 0;i < ct;i ++) {
        std::cout << "processing time " << i << " - " << end_time[i] << std::endl;
        outputPoints(start_time,end_time[i],"neigh");
    }

//    outputPolygons("../data/neighborhoods.txt", "neigh");
    exit(0);
#endif

#if 0
    testRasterJoin();
    exit(0);
#endif

    assert(sizeof(float) == sizeof(int) && sizeof(int) == sizeof(uint32_t));
    assert(sizeof(QueryConstraint) == 4 * sizeof(int));

    QStringList argString = app.arguments();
    QMap<QString, QString> args;
    for(int i = 1;i < argString.size();i ++) {
        QString arg1 = argString[i];
        if(arg1.startsWith("--")) {
            QString arg2 = " ";
            if((i + 1) < argString.length()) {
                arg2 = argString[i + 1];
            }
            if(arg2.startsWith("--")) {
                args[arg1] = " ";
            } else {
                args[arg1] = arg2;
            }
        }
    }
    if(!parseArguments(args)) {
        cout << "help message: see code!" << "\n";
        return 1;
    }

    attributes.clear();
    if(nAttrib > 0) {
        // Make sure it is only taxi data
        setupConstraints();
    }
    if(aggrAttrib != -1) {
        attributes.insert(aggrAttrib);
    }
    QElapsedTimer timer_all;
   // timer_all.start();
    setupExperiment();
    timer_all.start();
    std::cout<<"Now get into the next stage..."<<std::endl;
    runExperiment();
    experiment_time[7] += (experiment_time[0]+experiment_time[1]+experiment_time[2]+experiment_time[3]+experiment_time[4]+experiment_time[5]+experiment_time[6]);
    qDebug() << "experiment_time" << std::round(experiment_time[7]/10000.0f)/100.0f<<" milliseconds";
    std::cout<<"This is end"<<std::endl;
    std::cout<<"All time is"<<timer_all.elapsed() << " milliseconds" << std::endl;

    bool output = true;
    if(output) {

        string path = "experiment.csv";
        fstream fs;
        fs.open(path, ios::app);
        fs.setf(ios::fixed);
        fs<<title<<endl;
        for(int i=0;i<10;i++) {
            if (i<8) {
                experiment_time[i] = experiment_time[i] / static_cast<int>(nIter);
                fs<< std::fixed << std::setprecision(2) << experiment_time[i]/1000000.0f;
            } else {
                 fs<<experiment_time[i] ;
            }
            if(i!=9) {
                fs<<",";
            } else {
                fs<<endl;
            }
        }
        fs.close();
    }

    exit(0);
    return app.exec();
}
