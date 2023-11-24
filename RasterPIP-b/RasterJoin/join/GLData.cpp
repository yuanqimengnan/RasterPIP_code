#include <GL/glew.h>
#include "GLData.hpp"

#include <cassert>
#include <iostream>

void GLBuffer::generate(GLenum tgt, bool mapped) {
    glGenBuffers(1, &this->id);
    this->target = tgt;
    this->size = 0;
    this->mapped = mapped;
}

void GLBuffer::resize(GLenum usage, GLsizeiptr dataSize)
{
    if (this->size<dataSize) {
        glBindBuffer(this->target, this->id);
        glBufferData(this->target, dataSize, 0, usage);
        this->size = dataSize;
        GLenum err = glGetError();
        if(err != 0) {
            std::cout << "Memory error: " << err << " " << dataSize << "\n";
            exit(0);
        }

    }
}
// this->handler->attrBuf->setData(GL_DYNAMIC_DRAW,attribTypes.data(),3 * sizeof(GLfloat),0);
void GLBuffer::setData(GLenum usage, const GLvoid* data, GLsizeiptr dataSize, GLsizeiptr offset)
{
    if (dataSize==0) return;
    this->resize(usage, offset+dataSize);
    glBindBuffer(this->target, this->id);
    if(mapped) {
        void *dest = glMapBufferRange(this->target, offset, dataSize,
                                      GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT |
                                      (GL_MAP_INVALIDATE_RANGE_BIT));
        memcpy(dest, data, dataSize);
        glUnmapBuffer(this->target);
    } else {
        glBufferSubData(this->target, offset, dataSize, data);
    }
}

//  this->pbuffer->setData1(GL_DYNAMIC_DRAW, myId.data(), result_size * 4, result_size * 8, false);
void GLBuffer::setData1(GLenum usage, const GLvoid* data, GLsizeiptr dataSize, GLsizeiptr offset, bool flag)
{
    if (dataSize==0) return;
    // if(flag){this->resize(usage, dataSize*4); flag = false;}
    glBindBuffer(this->target, this->id);
    if(mapped) {
        void *dest = glMapBufferRange(this->target, offset, dataSize,
                                      GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT |
                                      (GL_MAP_INVALIDATE_RANGE_BIT));
        memcpy(dest, data, dataSize);
        glUnmapBuffer(this->target);
    } else {
        glBufferSubData(this->target, offset, dataSize, data);
        /**
         * target: 可以参考glBufferData中的描述，用来指定需要更新的缓冲区对象的类型
           offset: 指定了更新数据相对于缓冲区对象中原始数据开始位置的偏移量，也就是说要从什么地方开始更新原来的数据（以字节为单位）
           size：需要更新的数据量的大小
           data：一个指向新数据源的指针，将新的数据源拷贝到缓冲区对象中完成更新
         * 
         */
    }
}

void GLBuffer::setData(GLenum usage, const QByteArray *data, int numData)
{
    int totalMem = 0;
    for (int i=0; i<numData; i++)
        totalMem += data[i].size();

    uint8_t *arrayData = (uint8_t*)this->map(GL_STREAM_DRAW, totalMem);
    for (int i=0; i<numData; i++) {
        if (data[i].size()>0) {
            memcpy(arrayData, data[i].constData(), data[i].size());
            arrayData += data[i].size();
        }
    }
    this->unmap();
}

void * GLBuffer::map(GLenum usage, GLsizeiptr dataSize)
{
    if (dataSize==0) return 0;
    this->resize(usage, dataSize);
    return glMapBufferRange(this->target, 0, dataSize, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
}

void GLBuffer::unmap()
{
    glUnmapBuffer(this->target);
}

void GLBuffer::bind()
{
    glBindBuffer(this->target, this->id);
}

void GLBuffer::release()
{
    glBindBuffer(this->target, 0);
}

void GLTexture::bind()
{
    if (!this->size.isValid()) {
        glGenTextures(1, &this->id);
        glBindTexture(GL_TEXTURE_2D, this->id);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else
        glBindTexture(GL_TEXTURE_2D, this->id);
}

void GLTexture::release()
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLTexture::ensureSize(const QSize &newSize)
{
    {
        this->size = QSize(std::max(this->size.width(), newSize.width()),
                           std::max(this->size.height(), newSize.height()));
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     this->size.width(), this->size.height(),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    }
}

void GLTexture::setImage(const QImage &img)
{
    QImage texImg = QGLWidget::convertToGLFormat(img);
    this->bind();
    this->ensureSize(texImg.size());
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    texImg.width(), texImg.height(),
                    GL_RGBA, GL_UNSIGNED_BYTE, texImg.bits());
}


void GLTextureBuffer::create(int size, GLenum format, void* data)
{
    this->size = size;
    glGenBuffers(1, &ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_DYNAMIC_DRAW );
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo);
    glGenBuffers(1, &ssbo1);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo1);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_DYNAMIC_DRAW );
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, ssbo1);
    const GLuint zero = 0;
    glGenBuffers(1, &count);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, count);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof (GLint), &zero, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 3, count);
}

void GLTextureBuffer::setData(int size, GLenum format, void *data) {
    this->size = size;
    GLenum err;

    if( bufId <= 0 ) {
        qDebug() << "buffer not created!!";
        return;
    }

    glBindBuffer( GL_TEXTURE_BUFFER, bufId );
    glBufferData( GL_TEXTURE_BUFFER, size, data, GL_DYNAMIC_DRAW );

    err = glGetError();
    if( err > 0 ){
        QString strError;
        strError.sprintf("%s", glewGetErrorString(err));
        qDebug() << "set data TextureBuffer error 1: " << strError << err;
    }

    if( texId <= 0 ) {
        qDebug() << "texture buffer not created!!";
        return;
    }

    glBindTexture( GL_TEXTURE_BUFFER, texId );
    glTexBuffer( GL_TEXTURE_BUFFER, format,  bufId );
    glBindBuffer( GL_TEXTURE_BUFFER, 0 );
    glBindTexture( GL_TEXTURE_BUFFER, 0 );

    err = glGetError();
    if( err > 0 ){
        QString strError;
        strError.sprintf("%s", glewGetErrorString(err));
        qDebug() << "set data TextureBuffer error 2: " << strError << err;
    }
}

QVector<int> GLTextureBuffer::getBuffer()
{


//    glGetBufferSubData(GL_TEXTURE_BUFFER, 0, sizeof(int), &count);
//    GLenum err = glGetError();
//    QVector<int> data1(count);
    //Try to clear!!! 
    // glClearTexImage(texId, 0, GL_R, GL_INT, NULL);
    //  glClearTexImage(this->textureID, 0, GL_RG, GL_FLOAT, NULL);
    // glClearTexImage(bufId, 0, GL_RGBA, GL_FLOAT, NULL);
//    int count1 = data[0];
//    std::vector<char> emdata(size,0);
//    // glClearBufferSubData(bufId,GL_R32I,0,size,GL_RED_INTEGER ,GL_RED_INTEGER,emdata.data());
    
//    glBufferSubData(GL_TEXTURE_BUFFER, 0, size, emdata.data());
    // glDeleteTextures( 1, &texId); //delete previously created texture
    // glGenTextures( 1, &texId );
    // glBindTexture( GL_TEXTURE_BUFFER, texId );
    // glTexBuffer( GL_TEXTURE_BUFFER, GL_R32I,  bufId );
    // char arr[size];

//    // by liuziang
//    char *arr = new char[size];
//    glGetBufferSubData(GL_TEXTURE_BUFFER, 0, size, arr);

    // texBuf.create(128 * 3 * sizeof(GLint), GL_R32I, emdata.data());
    
    // glBufferSubData(bufId, 0, size, emdata.data());
    // glClearColor();
    //err = glGetError();
    // GLint errorid= glGetError();
    // glBindBuffer(GL_TEXTURE_BUFFER, 0);
    // glGetBufferSubData(GL_TEXTURE_BUFFER, 0, size, data.data());

    int count;
    glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(int), &count);
    QVector<int> data1(count);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, count*sizeof(int), data1.data());
    GLenum err = glGetError();
    if( err > 0 ){
        QString strError;
        strError.sprintf("%s", glewGetErrorString(err));
        qDebug() << "getBuffer error: " << strError;
        std::cout<< "getBuffer error: " << strError.toStdString();
    }
//    std::vector<char> emdata(size,0);
//    glBufferSubData(GL_TEXTURE_BUFFER, 0, size, emdata.data());
//    err = glGetError();
//    if( err > 0 ){
//        QString strError;
//        strError.sprintf("%s", glewGetErrorString(err));
//        qDebug() << "getBuffer error: " << strError;
//        std::cout<< "getBuffer error: " << strError.toStdString();
//    }
    return data1;
}

QVector<float> GLTextureBuffer::getBufferF()
{
    QVector<float> data(size / sizeof(float));
    glBindBuffer(GL_TEXTURE_BUFFER, bufId);
    GLenum err = glGetError();
    glGetBufferSubData(GL_TEXTURE_BUFFER, 0, size, data.data());
    err = glGetError();
    glBindBuffer(GL_TEXTURE_BUFFER, 0);

    err = glGetError();
    if( err > 0 ){
        QString strError;
        strError.sprintf("%s", glewGetErrorString(err));
        qDebug() << "getBuffer error: " << strError;
    }
    return data;
}

void GLTextureBuffer::destroy()
{
    if( bufId > 0 )
        glDeleteBuffers( 1, &bufId );  //delete previously created tbo

    if( texId > 0 )
        glDeleteTextures( 1, &texId); //delete previously created texture
    if( ssbo > 0 )
        glDeleteBuffers( 1, &ssbo );  //delete previously created tbo

    if( ssbo1 > 0 )
        glDeleteBuffers( 1, &ssbo1); //delete previously created texture
    if( count > 0 )
        glDeleteBuffers( 1, &count); //delete previously created texture

    bufId = 0;
    texId = 0;
    ssbo = 0;
    ssbo1 = 0;
    count = 0;
}

GLPersistentBuffer::~GLPersistentBuffer()
{
    glBindBuffer(target, id);
    glUnmapBuffer(target);
    glBindBuffer(target, 0);
    glDeleteBuffers(1,&id);
}

void GLPersistentBuffer::generate(GLenum tgt)
{
    glGenBuffers(1, &this->id);
    this->target = GL_ARRAY_BUFFER;
    this->size = 0;
}

void GLPersistentBuffer::resize(GLenum usage, GLsizeiptr bufferSize)
{
    glBindBuffer(target, id);
    if(bufferSize > size){
        if(pointer != NULL){
            glUnmapBuffer(target);
            pointer = NULL;
        }
        if(pointer == NULL) {
            glBindBuffer(target, this->id);
            glBufferStorage(target, bufferSize, 0, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
            pointer = glMapBufferRange(target, 0, bufferSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
            this->size = bufferSize;
        } else {
            assert(false);
        }
    }
}

void GLPersistentBuffer::setData(GLenum usage, const GLvoid *data, GLsizeiptr dataSize, GLsizeiptr offset)
{
    if(dataSize + offset > size) {
        qDebug() << "insufficient memory allocated" << size << (dataSize + offset);
        assert(false);
    }

    memcpy((char *)pointer + offset,data,dataSize);
}

void GLPersistentBuffer::bind()
{
    glBindBuffer(this->target, this->id);
}

void GLPersistentBuffer::release()
{
    glBindBuffer(this->target, 0);
}
