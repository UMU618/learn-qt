#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QLocale>
#include <QtCore/QTranslator>
#include <QtMultimedia/QAudioFormat>
#include <QtMultimedia/QAudioOutput>

// 准备 PCM 文件，可用 ffmpeg 转换：
// ffmpeg -i "X.mp3" -f s16le -ar 48000 -ac 2 test.pcm

// 播放：
// PcmPlayer test.pcm

int main(int argc, char* argv[]) {
  QCoreApplication app(argc, argv);
  qSetMessagePattern(
      "%{time yyyy-MM-ddThh:mm:ss.zzz} |%{pid}~%{threadid} <%{type}> "
      "%{message}");

  QTranslator translator;
  const QStringList kUiLanguages = QLocale::system().uiLanguages();
  for (const QString& locale : kUiLanguages) {
    const QString kBaseName = "PcmPlayer_" + QLocale(locale).name();
    if (translator.load("i18n/" + kBaseName)) {
      app.installTranslator(&translator);
      break;
    }
  }

  if (argc < 2) {
    qInfo() << QObject::tr("Usage: PcmPlayer pcm_file");
    return EXIT_SUCCESS;
  }

  auto pcm_file = new QFile(&app);
  pcm_file->setFileName(argv[1]);
  if (!pcm_file->open(QIODevice::ReadOnly)) {
    qCritical() << QObject::tr("Open PCM file failed!");
    return EXIT_FAILURE;
  }

  QAudioFormat audio_format;
  audio_format.setChannelCount(2);
  audio_format.setByteOrder(QAudioFormat::LittleEndian);
  audio_format.setCodec("audio/pcm");
  audio_format.setSampleRate(48000);
  audio_format.setSampleSize(16);
  audio_format.setSampleType(QAudioFormat::SignedInt);

  const QAudioDeviceInfo info(QAudioDeviceInfo::defaultOutputDevice());
  if (!info.isFormatSupported(audio_format)) {
    qCritical() << QObject::tr("Audio format is not supported!");
    return EXIT_FAILURE;
  }

  QAudioOutput* audio_output{new QAudioOutput(audio_format, &app)};
  audio_output->start(pcm_file);
  qDebug() << audio_output->error();
  audio_output->connect(audio_output, &QAudioOutput::stateChanged,
                        [&app, audio_output, pcm_file](QAudio::State state) {
                          if (QAudio::IdleState == state) {
                            audio_output->stop();
                            pcm_file->close();
                          } else if (QAudio::StoppedState == state) {
                            if (audio_output->error() != QAudio::NoError) {
                              qWarning() << "error:" << audio_output->error();
                            }
                            app.quit();
                          } else {
                            qDebug() << state;
                          }
                        });
  return app.exec();
}
