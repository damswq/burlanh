#include <QVideoSurfaceFormat>
#include <QtConcurrent/QtConcurrent>
#include "InferenceManager.h"

InferenceManager::~InferenceManager() {
    if (worker != nullptr) {
        worker->stop();
        worker->wait();
    }
}

QString InferenceManager::rootFolder() {
//    qDebug() << ROOT_FOLDER;
    return ROOT_FOLDER;
}

AsyncCameraCapture *InferenceManager::camera() {
    return m_camera;
}

void InferenceManager::setCamera(AsyncCameraCapture *camera) {
    qDebug() << "InferenceManager::setCamera";
    m_camera = camera;
    startWorkerIfReady();
}

AbstractVCamInterface *InferenceManager::virtualCamera() {
    return m_virtualCamera;
}

void InferenceManager::setVirtualCamera(AbstractVCamInterface *virtualCamera) {
    qDebug() << "InferenceManager::setVirtualCamera";
    if (m_virtualCamera == virtualCamera)
        return;
    m_virtualCamera = virtualCamera;
}

QAbstractVideoSurface *InferenceManager::videoSurface() {
    return m_videoSurface;
}

void InferenceManager::setVideoSurface(QAbstractVideoSurface *videoSurface) {
    qDebug() << "InferenceManager::setVideoSurface";
    m_videoSurface = videoSurface;
    startWorkerIfReady();
}

bool InferenceManager::mirror() const {
    return m_mirror;
}

void InferenceManager::setMirror(bool mirror) {
    qDebug() << "InferenceManager::setMirror" << mirror;
    if (m_mirror == mirror)
        return;
    m_mirror = mirror;
}

QString InferenceManager::avatarPath() const {
    return m_avatarPath;
}

void InferenceManager::setAvatarPath(const QString &avatarFilename) {
    qDebug() << "InferenceManager::setAvatarPath " << avatarFilename;

    QString avatarPath;
    if (avatarFilename != "none") {
        avatarPath = ROOT_FOLDER + "/.avatarify/avatars/" + avatarFilename;
    } else {
        avatarPath = avatarFilename;
    }

    if (m_avatarPath == avatarPath)
        return;

    m_avatarPath = avatarPath;
    QtConcurrent::run([this]() {
        QMutexLocker mutexLocker(&m_setAvatarPathMutex);
        if (worker != nullptr) {
            worker->setAvatarPath(m_avatarPath);
        }
    });
    Q_EMIT avatarPathChanged();
}

void InferenceManager::startWorkerIfReady() {
    if (m_camera != nullptr && m_videoSurface != nullptr) {
        if (worker == nullptr) {
            qDebug() << "Start worker!";

            QVideoSurfaceFormat format(QSize(1280, 720), QVideoFrame::PixelFormat::Format_ARGB32);
            m_videoSurface->start(format);

            worker.reset(new InferenceWorker(m_camera));
            connect(m_camera, &AsyncCameraCapture::present, worker.data(), &InferenceWorker::setFrame, Qt::DirectConnection);
            connect(worker.data(), &InferenceWorker::present, this, &InferenceManager::presentFrame);
            connect(worker.data(), &QThread::finished, worker.data(), &QObject::deleteLater);
            worker->start();
        } else {
            qDebug() << "Refuse to start worker, worker is already started.";
        }
    } else {
        qDebug() << "Refuse to start worker, not all params are filled.";
    }
}

void InferenceManager::requestCalibration() {
    if (worker != nullptr) {
        worker->requestCalibration();
    }
}

void InferenceManager::presentFrame(const QImage &generatedFrame) {
    QImage img(QSize(1280, 720), /*QImage::Format_ARGB32*/generatedFrame.format());
    QPainter painter(&img);
    auto frame = generatedFrame.scaled(720, 720);
    if (m_mirror) {
        frame = frame.mirrored(true, false);
    }
    painter.drawImage(QPointF((1280 - 720) / 2, 0), frame);///*generatedFrame.convertToFormat(QImage::Format_ARGB32).scaled(480, 480)*/);
    // vcam
    m_virtualCamera->present(img);

    // preview
    QVideoFrame previewFrame(img.convertToFormat(QImage::Format_ARGB32));
    m_videoSurface->present(previewFrame);
}
