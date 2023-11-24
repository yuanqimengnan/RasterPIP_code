#include <GL/glew.h>
#include "GLFunction.hpp"
#include "Utils.h"
#include "GLHandler.hpp"
#include "UsefulFuncs.hpp"
#include "forexperiment.h"

#include <QElapsedTimer>
using namespace std;
GLFunction::GLFunction(GLHandler *handler): handler(handler), dirty(true)
{
    // default 100 meters;
    this->cellSize = (100.0 / getGroundResolution());
    gvao = 0;
    useGL = true;
}

GLFunction::~GLFunction() {}

void GLFunction::setupForRender() {
    if(dirty) {
        this->updateBuffers();
    }
    dirty = false;
}

void GLFunction::setMaxCellSize(double size) {
    // size in meters
    size /= std::sqrt(2);
    cellSize = (size / getGroundResolution());
}

QVector<int> GLFunction::execute() {
#ifdef PROFILE_GL
    ptMemTime.append(0); ptRenderTime.append(0); backendQueryTime.append(0);
    polyMemTime.append(0); polyRenderTime.append(0);
    polyIndexTime.append(0); triangulationTime.append(0);

    QElapsedTimer timer;
    timer.start();
#endif
    polySize = this->handler->dataHandler->getPolyHandler()->getNoPolys();
    // result = QVector<int>(polySize * 3,0);
    // result = QVector<int>(result_size*4);
    this->setupForRender();
#ifdef PROFILE_GL
    qint64 elapsed = timer.elapsed();
    setupTime.append(elapsed);
#endif
    return this->executeFunction();
}

void GLFunction::initGL() {
    indexSizeShader.reset(new QOpenGLShaderProgram());
    indexSizeShader->addShaderFromSourceFile(QOpenGLShader::Compute, ":shaders/index-size.glsl");
    indexSizeShader->link();

    indexShader.reset(new QOpenGLShaderProgram());
    indexShader->addShaderFromSourceFile(QOpenGLShader::Compute, ":shaders/create-index.glsl");
    indexShader->link();
}

void GLFunction::updateBuffers() {
}


void GLFunction::createPolyIndex() {
    int localSize = 1024;
    indexCreated = false;
    DataHandler * data = this->handler->dataHandler;
    PolyHandler *poly = data->getPolyHandler();

#ifdef PROFILE_GL
    QElapsedTimer timer;
    timer.start();
#endif
    int noPolys = poly->getNoPolys();
    int xs = this->handler->presx; int ys = this->handler->presx;
    Bound bound = poly->getBounds();
    QPointF diff = bound.rightTop - bound.leftBottom;
    headSize = sizeof(int) * xs * ys;

    polyBufferGL.create(poly->polys[poly->currentCollection].size() * sizeof(float), GL_RG32F, poly->polys[poly->currentCollection].data());
    pindexBufferGL.create(poly->pindexes[poly->currentCollection].size() * sizeof(int), GL_R32I,poly->pindexes[poly->currentCollection].data());
    headIndexGL.create(headSize,GL_R32I,0);

    // compute size of index
    int memRequired = 0;
    indexSizeBuf.create(sizeof(GLint),GL_R32I,&memRequired);

    indexSizeShader->bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, polyBufferGL.texId);
    glUniform1i(indexSizeShader->uniformLocation("polys"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, pindexBufferGL.texId);
    glUniform1i(indexSizeShader->uniformLocation("pindex"), 1);

    glUniform2f(indexSizeShader->uniformLocation("leftBottom"), bound.leftBottom.x(), bound.leftBottom.y());
    glUniform2f(indexSizeShader->uniformLocation("cellSize"), diff.x() / xs, diff.y() / ys);
    glUniform2i(indexSizeShader->uniformLocation("res"),xs,ys);

    indexSizeShader->setUniformValue("noPolys", (int)noPolys);

    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, indexSizeBuf.bufId);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, indexSizeBuf.bufId);

    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, headIndexGL.bufId);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, headIndexGL.bufId);

    int workSize = int(std::ceil(float(xs * ys) / localSize));
    glDispatchCompute(workSize,1,1);

    // Create the index itself
    memRequired = indexSizeBuf.getBuffer()[0];
    GLBuffer atomicCounter;
    GLuint counter = 0;
    atomicCounter.generate(GL_ATOMIC_COUNTER_BUFFER, true);
    atomicCounter.resize(GL_DYNAMIC_DRAW, sizeof(GLuint));
    atomicCounter.setData(GL_DYNAMIC_DRAW,&counter,0);

    linkedListSize = sizeof(int) * 2 * memRequired;
    linkedListGL.create(linkedListSize,GL_RG32I,0);

    indexShader->bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, polyBufferGL.texId);
    glUniform1i(indexShader->uniformLocation("polys"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, pindexBufferGL.texId);
    glUniform1i(indexShader->uniformLocation("pindex"), 1);

    glUniform2f(indexShader->uniformLocation("leftBottom"), bound.leftBottom.x(), bound.leftBottom.y());
    glUniform2f(indexShader->uniformLocation("cellSize"), diff.x() / xs, diff.y() / ys);
    glUniform2i(indexShader->uniformLocation("res"),xs,ys);
    indexShader->setUniformValue("noPolys", (int)noPolys);

    glBindBufferARB(GL_ATOMIC_COUNTER_BUFFER, atomicCounter.id);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomicCounter.id);

    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, headIndexGL.bufId);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, headIndexGL.bufId);

    glBindBufferARB(GL_SHADER_STORAGE_BUFFER, linkedListGL.bufId);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, linkedListGL.bufId);

    workSize = int(std::ceil(float(noPolys) / localSize));
    glDispatchCompute(workSize,1,1);
    glFinish();
    indexCreated = true;
#ifdef PROFILE_GL
    qint64 time = timer.elapsed();
    polyIndexTime.last() += time;
#endif
}

void GLFunction::setupPolygons() {
    DataHandler * data = this->handler->dataHandler;
    PolyHandler * poly = data->getPolyHandler();

#ifdef PROFILE_GL
    QElapsedTimer timer;
    timer.start();
#endif

    poly->getTriangulation(verts,ids);

#ifdef PROFILE_GL
    
    qint64 triTime = timer.elapsed();
    //liuziang:for experiment output
    experiment_time[1] += timer.nsecsElapsed();
    qDebug()<<"三角化的时间"<<std::round(timer.nsecsElapsed() / 10000.0f) / 100.0f<<"ms";
    triangulationTime.last() += triTime;
//    cout<<verts.size()<<endl;
//    timer.restart();
#endif

    // uint32_t nopolys = poly->getNoPolys();
    nopolys = poly->getNoPolys();
    GLsizeiptr memRequired = verts.size() * sizeof(float) // vertices of triangles
                       + ids.size() * sizeof(int)         // 1 id per vertex
                       + 3 * nopolys * sizeof(int);       // output buffer size
  //  qDebug() << "required memory size of polygons is" << memRequired << memLeft;
    if(memRequired > memLeft) {
        qDebug() << "polygons do not fit in memory. case not implemented!!" << memRequired << memLeft;
        return;
    }

//    // loading polygon into memory
    psize = verts.size() * sizeof(float);
//    this->polyBuffer->resize(GL_DYNAMIC_DRAW,psize*3/2);

//    this->polyBuffer->setData(GL_DYNAMIC_DRAW,verts.data(),psize,0);
//    this->polyBuffer->setData(GL_DYNAMIC_DRAW,ids.data(),psize / 2,psize);//这是传id值吧
//    //liuziang:for experiment output
//    qDebug()<<"polygon loading"<<std::round(timer.nsecsElapsed() / 10000.0f) / 100.0f<<"ms";
//    experiment_time[2] += timer.nsecsElapsed();

    memLeft -= memRequired;
#ifdef PROFILE_GL
    glFinish();
    polyMemTime.last() += timer.elapsed();
#endif
}
/**
void GLFunction::clearFrame() {//自己写的一个清空的函数，现在用不上了
    // uint32_t nopolys = data->getPolyHandler()->getNoPolys();
    texBuf.create(128 * 3 * sizeof(GLint), GL_R32I, result.data());
    glFinish();
}**/
void GLFunction::setupPoints() {
#ifdef PROFILE_GL
    QElapsedTimer timer;
    timer.start();
#endif

    // Get attributes for query
    DataHandler * data = this->handler->dataHandler;
    QueryResult* res = data->getCoarseQueryResult();

    // TODO fix a format and get query too. hardcoding for now
    // other attributes should also include the attribute on which aggregation is performed. reduces redundancy.
    set<size_t> other_attributes;
    size_t location_attribute = res->getLocationAttributeId();

    // Format array of triples: <attribute id> <constraint type> <value>
    // All stored as floats for easy manipulation, assume
    vector<QueryConstraint> constraints = data->getQueryConstraints();
    int noConstraints = constraints.size();

    inputData.clear();
    attributeMap.clear();
    attribTypes.clear();
    attribSizes.clear();
    attrCt = 0;

    uint32_t result_size; //number of records in the result
    int record_size = 0;

    // primary spatial attribute = 0
    attribSizes << 2;
    attribTypes << Location << 0 << 0 << 0;
    attributeMap[location_attribute] = attrCt;
    attrCt ++;

    ByteArray * binAttribResult = res->getAttribute(location_attribute);// ByteArray :vector<char>*
    result_size = binAttribResult->size() /(3*sizeof(float));

//    string selectFile = "/home/lza/DATA/EXPERIMENT/Coarse_user_raster.txt";
//    ofstream outfile;
//    outfile.open(selectFile, ios::out);

//    if (!outfile.is_open())
//    {
//        qDebug() << "oh no";
//    }
   

//    int id = 0;
//    float lat,lon;
//    char* i1p, *f1p, *f2p;
//    char * begin1,*begin2;
//    //   mybin = new vector<char>(8*lastBatch);
//    ByteArray* mybin = new vector<char>(8*result_size); //typedef std::vector<char> ByteArray;
//    myId = vector<int>(result_size);
//    IDss = vector<int>(result_size);
 //为了存VAO
//    for(int i =0; i < result_size; i++) {
//        i1p = &(*binAttribResult)[i*12];
//        f1p = &(*binAttribResult)[i*12 + 4];
//        f2p = &(*binAttribResult)[i*12 + 8];
//        id = *(int*)(i1p);
//        lon = *(float*)(f1p);
//        lat  = *(float*)(f2p);
//       // IDs[i] = id;
//        IDss[i] = id;
//        begin1 = reinterpret_cast<char*>(&lon);
//        begin2 = reinterpret_cast<char*>(&lat);
//        memcpy(mybin->data() + i*8,  begin1, 4);
//        memcpy(mybin->data() + i*8+4, begin2, 4);
//    }

//    inputData<<mybin;
    // assert(cnt == mybin->size());
    // sort(myId.begin(),myId.end());
    // for (int i = 0; i < myId.size(); i++)
    // {
    //     outfile<<myId[i]<<endl;  
    // }
    // outfile.close();
//    std::cout<<"Coarse Query points number is"<<result_size<<std::endl;
    //liuziang:for experiment output
    experiment_time[8] = result_size;

//    result = QVector<int>(result_size);
//    result[0] = 0;


    record_size += 2;


    Aggregation aggrType;
    int aggrAttrib;
    data->getAggregation(aggrType,aggrAttrib);
    assert(aggrType == Count || (aggrType != Count && aggrAttrib != -1));
    aggr = aggrType;

    if(aggrType != Count) {
        other_attributes.insert(aggrAttrib);
    }
    this->noConstraints = noConstraints;

#ifdef PROFILE_GL
    qint64 btime = timer.elapsed();
    this->backendQueryTime.last() += btime;
    timer.restart();
#endif
    // creating output buffer.
//    uint32_t nopolys = data->getPolyHandler()->getNoPolys();
    // texBuf.create(nopolys * 3 * sizeof(GLint), GL_R32I, result.data());
    result = QVector<int>(result_size*5);
    texBuf.create((result_size)  * sizeof(GLint), GL_R32I, result.data());

    // memLeft -= nopolys * 3 * sizeof(GLint);
    memLeft -= result_size* sizeof(GLint);

    gpu_size = memLeft / (record_size * sizeof(float)); //number of records gpu buffer can fit.
    int number_of_passes = (int)ceil((result_size+0.0)/(gpu_size+0.0));

    gpu_size = (int)ceil((result_size+0.0) / (number_of_passes + 0.0));
    tsize = (result_size > gpu_size) ? gpu_size : result_size;

    ptsSize = result_size;
    noPtPasses = number_of_passes;
    ptsRecordSize = record_size * sizeof(int);

#ifdef PROFILE_GL
    glFinish();
    this->ptMemTime.last() += timer.elapsed();
#endif
}

void GLFunction::setupPolygonIndex() {
    PolyHandler *poly = this->handler->dataHandler->getPolyHandler();
    memLeft -= (headSize + linkedListSize + poly->polys[poly->currentCollection].size() * sizeof(float)
            + poly->pindexes[poly->currentCollection].size() * sizeof(int));
    memLeft -= (poly->getNoPolys() * sizeof(int) * 3);
}
