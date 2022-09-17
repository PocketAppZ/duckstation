#include "coverdownloaddialog.h"
#include "common/assert.h"
#include "frontend-common/game_list.h"

CoverDownloadDialog::CoverDownloadDialog(QWidget* parent /*= nullptr*/) : QDialog(parent)
{
  m_ui.setupUi(this);
  updateEnabled();

  connect(m_ui.start, &QPushButton::clicked, this, &CoverDownloadDialog::onStartClicked);
  connect(m_ui.close, &QPushButton::clicked, this, &CoverDownloadDialog::onCloseClicked);
  connect(m_ui.urls, &QTextEdit::textChanged, this, &CoverDownloadDialog::updateEnabled);
}

CoverDownloadDialog::~CoverDownloadDialog()
{
  Assert(!m_thread);
}

void CoverDownloadDialog::closeEvent(QCloseEvent* ev)
{
  cancelThread();
}

void CoverDownloadDialog::onDownloadStatus(const QString& text)
{
  m_ui.status->setText(text);
}

void CoverDownloadDialog::onDownloadProgress(int value, int range)
{
  // limit to once every five seconds
  if (m_last_refresh_time.GetTimeSeconds() >= 5.0f)
  {
    emit coverRefreshRequested();
    m_last_refresh_time.Reset();
  }

  if (range != m_ui.progress->maximum())
    m_ui.progress->setMaximum(range);
  m_ui.progress->setValue(value);
}

void CoverDownloadDialog::onDownloadComplete()
{
  emit coverRefreshRequested();

  if (m_thread)
  {
    m_thread->join();
    m_thread.reset();
  }

  updateEnabled();
  
  m_ui.status->setText(tr("Download complete."));
}

void CoverDownloadDialog::onStartClicked()
{
  if (m_thread)
    cancelThread();
  else
    startThread();
}

void CoverDownloadDialog::onCloseClicked()
{
  if (m_thread)
    cancelThread();

  done(0);
}

void CoverDownloadDialog::updateEnabled()
{
  const bool running = static_cast<bool>(m_thread);
  m_ui.start->setText(running ? tr("Stop") : tr("Start"));
  m_ui.start->setEnabled(running || !m_ui.urls->toPlainText().isEmpty());
  m_ui.close->setEnabled(!running);
  m_ui.urls->setEnabled(!running);
}

void CoverDownloadDialog::startThread()
{
  m_thread = std::make_unique<CoverDownloadThread>(this, m_ui.urls->toPlainText());
  connect(m_thread.get(), &CoverDownloadThread::statusUpdated, this, &CoverDownloadDialog::onDownloadStatus);
  connect(m_thread.get(), &CoverDownloadThread::progressUpdated, this, &CoverDownloadDialog::onDownloadProgress);
  connect(m_thread.get(), &CoverDownloadThread::threadFinished, this, &CoverDownloadDialog::onDownloadComplete);
  m_thread->start();
  updateEnabled();
}

void CoverDownloadDialog::cancelThread()
{
  if (!m_thread)
    return;

  m_thread->requestInterruption();
  m_thread->join();
  m_thread.reset();
}

CoverDownloadDialog::CoverDownloadThread::CoverDownloadThread(QWidget* parent, const QString& urls)
  : QtAsyncProgressThread(parent)
{
  for (const QString& str : urls.split(QChar('\n')))
    m_urls.push_back(str.toStdString());
}

CoverDownloadDialog::CoverDownloadThread::~CoverDownloadThread() = default;

void CoverDownloadDialog::CoverDownloadThread::runAsync()
{
  GameList::DownloadCovers(m_urls, this);
}