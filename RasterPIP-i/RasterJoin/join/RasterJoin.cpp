#include <GL/glew.h>
#include "RasterJoin.hpp"
#include "GLHandler.hpp"
#include "Utils.h"
#include "forexperiment.h"
#include <QOpenGLVertexArrayObject>
#include "QueryResult.hpp"
#include <QElapsedTimer>
#include <set>
#include <list>
#include <omp.h>
#include "UsefulFuncs.hpp"
#include <bits/stdc++.h>
#include <GLData.hpp>

RasterJoin::RasterJoin(GLHandler *handler) : GLFunction(handler)
{
}

RasterJoin::~RasterJoin() {}

void RasterJoin::initGL()
{
    // init shaders
    pointsShader.reset(new QOpenGLShaderProgram());

    pointsShader->addShaderFromSourceFile(QOpenGLShader::Vertex, ":shaders/points.vert");
    pointsShader->addShaderFromSourceFile(QOpenGLShader::Fragment, ":shaders/points.frag");
    pointsShader->link();

    polyShader.reset(new QOpenGLShaderProgram());
    polyShader->addShaderFromSourceFile(QOpenGLShader::Vertex, ":shaders/polygon.vert");
    polyShader->addShaderFromSourceFile(QOpenGLShader::Fragment, ":shaders/polygon.frag");
    polyShader->link();

    if (this->gvao == 0)
    {
        glGenVertexArrays(1, &this->gvao);
    }

    pbuffer = new GLBuffer();
    pbuffer->generate(GL_ARRAY_BUFFER, !(handler->inmem));

    polyBuffer = new GLBuffer();
    polyBuffer->generate(GL_ARRAY_BUFFER);

    GLint dims;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &dims);
    this->maxRes = std::min(MAX_FBO_SIZE, dims);

    glGenQueries(1, &query);

#ifdef FULL_SUMMARY_GL
    qDebug() << "setup buffers and shaders for raster join";
#endif
}

void RasterJoin::updateBuffers()
{
    PolyHandler *poly = this->handler->dataHandler->getPolyHandler();
    Bound bound = poly->getBounds();
    QPointF diff = bound.rightTop - bound.leftBottom;

    actualResX = int(std::ceil(diff.x() / cellSize));
    actualResY = int(std::ceil(diff.y() / cellSize));

    splitx = std::ceil(double(actualResX) / maxRes);
    splity = std::ceil(double(actualResY) / maxRes);

    resx = std::ceil(double(actualResX) / splitx);
    resy = std::ceil(double(actualResY) / splity);

    GLFunction::size = QSize(resx, resy);
    if (this->polyFbo.isNull() || this->polyFbo->size() != GLFunction::size)
    {
        this->polyFbo.clear();
        this->pointsFbo.clear();

        this->polyFbo.reset(new FBOObject(GLFunction::size, FBOObject::NoAttachment, GL_TEXTURE_2D, GL_RGBA32F));
        GLenum id = glGetError();
        this->pointsFbo.reset(new FBOObject(GLFunction::size, FBOObject::NoAttachment, GL_TEXTURE_2D, GL_RGBA32F));
    }
}

inline GLuint64 getTime(GLuint query)
{
    int done = 0;
    while (!done)
    {
        glGetQueryObjectiv(query, GL_QUERY_RESULT_AVAILABLE, &done);
    }
    GLuint64 elapsed_time;
    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &elapsed_time);
    return elapsed_time;
}

// TODO:
// this->renderPoints(offset, (int)(result_size)); //多加一个偏移量？
void RasterJoin::renderPoints(int offset, int sum, bool flag)
{

    this->pointsFbo->bind();

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    pointsShader->bind();

    // send query constraints
    // GLuint queryBindingPoint = 0;
    // glBindBufferBase(GL_UNIFORM_BUFFER, queryBindingPoint, this->handler->queryBuf->id);
    // GLuint ind = glGetUniformBlockIndex(pointsShader->programId(), "queryBuffer");
    // glUniformBlockBinding(pointsShader->programId(), ind, queryBindingPoint);

    // attribute type
    // GLuint attrBindingPoint = 1;
    // glBindBufferBase(GL_UNIFORM_BUFFER, attrBindingPoint, this->handler->attrBuf->id);
    // ind = glGetUniformBlockIndex(pointsShader->programId(), "attrBuffer");
    // glUniformBlockBinding(pointsShader->programId(), ind, attrBindingPoint);

    // aggrType - of aggregation. 0 - count, sum of attribute id for corresponding attribute
    // pointsShader->setUniformValue("aggrType", aggr);
    // pointsShader->setUniformValue("aggrId", aggrId);
    // pointsShader->setUniformValue("attrCt", attrCt);
    pointsShader->setUniformValue("mvpMatrix", mvp);
    pointsShader->setUniformValue("clear_flag", clear_flag);
    // pointsShader->setUniformValue("queryCt", noConstraints);

    glBindVertexArray(this->gvao);
    this->pbuffer->bind();
    // GLsizeiptr offset = 0;
    // for(int j = 0;j < inputData.size();j ++) {
    //     // pointsShader->setAttributeBuffer(j,GL_FLOAT,offset,attribSizes[j]);//
    //     pointsShader->enableAttributeArray(j);
    //     pointsShader->setAttributeBuffer(j,GL_FLOAT,0,2);

    //     // offset += tsize * attribSizes[j] * sizeof(float);

    // }
//    pointsShader->setAttributeBuffer(0, GL_FLOAT, 0, 2);
//    pointsShader->enableAttributeArray(0);
//    pointsShader->setAttributeBuffer(1, GL_FLOAT, sum * 2 * sizeof(float), 1);
//    pointsShader->enableAttributeArray(1);
    pointsShader->setAttributeBuffer(0, GL_FLOAT, sizeof (int),2 ,sizeof(int)+2*sizeof(float));
    pointsShader->enableAttributeArray(0);
    pointsShader->setAttributeBuffer(1, GL_FLOAT, 0,1, sizeof(int)+2*sizeof(float));
    pointsShader->enableAttributeArray(1);
    // pointsShaderpoint->setAttributeBuffer(1, GL_FLOAT, 2699712, 1);
    //    polyShader->enableAttributeArray(1);
    glActiveTexture(GL_TEXTURE0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, this->polyFbo->texture());
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // glUniform1i(this->pointsShader->uniformLocation("PolyTex"), 0);
    glBindImageTexture(0, texBuf.texId, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32I);

#ifdef PROFILE_GL
    glBeginQuery(GL_TIME_ELAPSED, query);
#endif

    // glDrawArrays(GL_POINTS, 0, tsize);
    // glDrawArrays(GL_POINTS, offset, sum);
    glDrawArrays(GL_POINTS, 0, sum);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

#ifdef PROFILE_GL
    glEndQuery(GL_TIME_ELAPSED);
    glFinish();
    GLuint64 elapsed_time = getTime(query);
    this->ptRenderTime.last() += (elapsed_time / 1000000);
#endif

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    FBOObject::bindDefault();
}

void RasterJoin::renderPolys(int sum)
{
    this->polyFbo->bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
  //  glBlendFunc(GL_ONE, GL_ZERO);

    this->polyShader->bind();
    polyShader->setUniformValue("mvpMatrix", mvp);
    // polyShader->setUniformValue("offset",polySize);
    // polyShader->setUniformValue("aggrType", aggr);
    glBindVertexArray(this->gvao);
    this->polyBuffer->bind();
    polyShader->setAttributeBuffer(0, GL_FLOAT, 0, 2);
    polyShader->setAttributeBuffer(1, GL_FLOAT, sum * 2 * sizeof(float), 1);//试图用做分批
    polyShader->enableAttributeArray(0);
    polyShader->enableAttributeArray(1);

    // glActiveTexture(GL_TEXTURE0);
    // glBindTexture(GL_TEXTURE_2D, this->pointsFbo->texture());
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // glUniform1i(this->polyShader->uniformLocation("pointsTex"), 0);
    // glBindImageTexture(0, texBuf.texId, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32I);

#ifdef PROFILE_GL
    glBeginQuery(GL_TIME_ELAPSED, query);
#endif
    glDrawArrays(GL_TRIANGLES, 0, sum);
    // glDisableVertexAttribArray(0);

#ifdef PROFILE_GL
    glEndQuery(GL_TIME_ELAPSED);
    glFinish();
    GLuint64 elapsed_time = getTime(query);
    this->polyRenderTime.last() += (elapsed_time / 1000000);
#endif
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void RasterJoin::performJoin()
{

    GLsizeiptr origMem = memLeft;
    this->setupPolygons();
    this->setupPoints();

    // setup rendering passes
    Bound bound = this->handler->dataHandler->getPolyHandler()->getBounds();
    QPointF diff = bound.rightTop - bound.leftBottom;
    diff.setX(diff.x() / splitx);
    diff.setY(diff.y() / splity);
    uint32_t result_size = ptsSize;
    GLsizeiptr passOffset = 0;
    bool flag = true;
    vector<int> pos;

    DataHandler * data = this->handler->dataHandler;
    QueryResult* res = data->getCoarseQueryResult();
    size_t location_attribute = res->getLocationAttributeId();
    ByteArray * binAttribResult = res->getAttribute(location_attribute);// ByteArray :vector<char>*
    result_size = binAttribResult->size() /(3*sizeof(float));
    std::cout<<"Coarse Query points number is"<<result_size<<std::endl;

    QElapsedTimer t1;
    t1.start();
    this->pbuffer->resize(GL_DYNAMIC_DRAW, (result_size * 2 * 4) + (result_size * 4));
    this->pbuffer->setData1(GL_DYNAMIC_DRAW, binAttribResult->data(), result_size * 12, 0, false);
    experiment_time[3] += t1.nsecsElapsed();
    qDebug() << "pass points " << std::round(t1.nsecsElapsed() / 10000.0f) / 100.0f<< " milliseconds";

    int allNodes = 0;
    int allNodestest = 0;
    vector<int> TrueId;

    // group by polygon id
    std::vector<std::vector<int>> polygonArray(this->polySize);
    int batch_number = static_cast<int>(ceil(static_cast<float>(this->polySize) / batch_size));
    PolyHandler * poly = data->getPolyHandler();
    unsigned int tmp = 0;

    int mts = omp_get_max_threads();
    QElapsedTimer timer1;
    QElapsedTimer t3;
    QElapsedTimer t4;
    qint64 time_of_render_polygons;
    qint64 time_of_render_points;
    //each thread has its own array to store results
    std::vector<std::vector<std::vector<int>>> polygonArray_thread(mts, std::vector<std::vector<int>>(this->polySize));

    GLvoid* mappedData =  glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,result_size*5*sizeof(int),GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_MAP_READ_BIT);
    GLenum error = glGetError();

    //render
    for (int i = 0; i < batch_number; i++)
    {
        GLsizeiptr offset = 0;
        clear_flag = true;

        unsigned int batch_polygons_size = 0;
        // loading polygon into memory
        timer1.start();
        if (i!=batch_number-1) {
            for (int m=0;m<batch_size;m++) {
                batch_polygons_size += poly->polyssize[m + i*batch_size];
            }
            this->polyBuffer->resize(GL_DYNAMIC_DRAW,batch_polygons_size*3*sizeof(float));
            this->polyBuffer->setData(GL_DYNAMIC_DRAW,verts.data()+tmp*2,batch_polygons_size*2*sizeof(float),0);
            this->polyBuffer->setData(GL_DYNAMIC_DRAW,ids.data()+tmp,batch_polygons_size*sizeof(float),batch_polygons_size*2*sizeof(float));//这是传id值吧
            tmp += batch_polygons_size;
        } else {
            int left = this->polySize - i * batch_size;
            for (int m=0;m<left;m++) {
                batch_polygons_size += poly->polyssize[m + i*batch_size];
            }
            this->polyBuffer->resize(GL_DYNAMIC_DRAW,batch_polygons_size*3*sizeof(float));
            this->polyBuffer->setData(GL_DYNAMIC_DRAW,verts.data()+tmp*2,batch_polygons_size*2*sizeof(float),0);
            this->polyBuffer->setData(GL_DYNAMIC_DRAW,ids.data()+tmp,batch_polygons_size*sizeof(float),batch_polygons_size*2*sizeof(float));//传id值
            tmp += batch_polygons_size;
        }
        experiment_time[2] += timer1.nsecsElapsed();

        QElapsedTimer t3;
        time_of_render_polygons = 0;
        time_of_render_points = 0;
        //render
        for (int x = 0; x < splitx; x++)
        {
            for (int y = 0; y < splity; y++)
            {
                QPointF lb = bound.leftBottom + QPointF(x * diff.x(), y * diff.y());
                QPointF rt = lb + diff;
                this->mvp = getMVP(lb, rt);
                t3.start();
                this->renderPolys(batch_polygons_size);
                time_of_render_polygons += t3.nsecsElapsed();
                t3.restart();
                this->renderPoints(offset, (int)(result_size)); //多加一个偏移量？
                time_of_render_points += t3.nsecsElapsed();
                clear_flag = false;
            }
        }
        //get result and map id
        t4.start();
        int count;
        glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(int), &count);
        result.resize(count);
        memcpy(result.data(), mappedData, count * sizeof(int));
        count = 0;
        glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(int), &count);
//        test1 += t4.elapsed();
//        t4.restart();

        //ID map in one array
        int my_result_size = result.size();
        if (my_result_size != 0) {
            #pragma omp parallel for
            for (int m=0;m<my_result_size;m=m+5) {
                int id = omp_get_thread_num();
                for (int n=0;n<4;n++) {
                    int batch_polyids = result[m + n + 1];
                    int polyid = 0;
                    while (batch_polyids > 0) {
                        if (batch_polyids & 1) {
                            int true_poly_id = 0;
                            true_poly_id = polyid + n * single_pass_size + i * batch_size;
//                            #pragma omp critical
//                            polygonArray[true_poly_id].push_back(result[m]);
                            polygonArray_thread[id][true_poly_id].push_back(result[m]);
                        }
                        batch_polyids >>= 1;
                        polyid++;
                    }
                }
            }
        }
//        test2 += t4.elapsed();

        experiment_time[6] += t4.nsecsElapsed();
        experiment_time[4] += time_of_render_polygons;
        experiment_time[5] += time_of_render_points;
        allNodes += my_result_size/5;
    }
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
 //   format the groupby result
    t4.restart();
    for (int i=0;i<mts;i++) {
        for (int t=0;t<int(this->polySize);t++) {
            polygonArray[t].insert(polygonArray[t].end(), polygonArray_thread[i][t].begin(), polygonArray_thread[i][t].end());
            allNodestest += polygonArray_thread[i][t].size();
        }
    }
    experiment_time[6] += t4.nsecsElapsed();
    experiment_time[9] = allNodestest;

    qDebug() << "pass polygons " << std::round(experiment_time[2] / 10000.0f) / 100.0f << " milliseconds";
    qDebug() << "render Polygons " << std::round(experiment_time[4] / 10000.0f) / 100.0f << " milliseconds";
    qDebug() << "render Points " << std::round(experiment_time[5] / 10000.0f) / 100.0f << " milliseconds";
    qDebug() << "get result " << std::round(experiment_time[6] / 10000.0f) / 100.0f << " milliseconds";
    qDebug() << "The result size is" << allNodes;
    qDebug() << "group result size is" << allNodestest;
    qDebug() << "------------------------------------------";

//    Output the result into file, if needed.
//    string selectFile = "result.txt";
//    ofstream outfile;
//    outfile.open(selectFile, ios::out);

//    if (!outfile.is_open())
//    {
//        qDebug() << "open outfile failed";
//    }
//    // vector<int> myId = std::vector<int>(pos.begin(), pos.end());
//    sort(result.begin(),result.end());
//    for (int i = 0; i < result.size(); i++)
//    {
//        outfile << result[i] << endl;
//    }
//    outfile.close();

    texBuf.destroy();
    memLeft = origMem;
}

QVector<int> RasterJoin::executeFunction()
{
    QElapsedTimer timer;
    timer.start();

    glViewport(0, 0, pointsFbo->width(), pointsFbo->height());

    // setup buffer for storing results
    memLeft = this->handler->maxBufferSize;

    // assume polygon data always fits in gpu memory!
    performJoin();

    executeTime.append(timer.elapsed());

    return result;
}
