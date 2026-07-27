
//  WordCountJournal
//  Copyright 2013-2026 Hal Canary
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include <type_traits>

#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtGui/QCloseEvent>
#include <QtGui/QFont>
#include <QtGui/QFontDatabase>
#include <QtGui/QScreen>
#include <QtGui/QShortcut>
#include <QtGui/QTextBlock>
#include <QtGui/QTextDocument>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QTextEdit>

////////////////////////////////////////////////////////////////////////////////

static int getWordCount(QTextDocument *doc) {
    int count = 0;
    bool inWord = false;
    for (QTextBlock block = doc->begin(); block.isValid();
         block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            QTextFragment fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            const QString &fragText = fragment.text();
            for (int i = 0; i < fragText.length(); ++i) {
                if (fragText.at(i).isSpace()) {
                    inWord = false;
                } else if (!inWord) {
                    inWord = true;
                    count++;
                }
            }
        }
        inWord = false;
    }
    return count;
}

static void saveTextFile(QString text, QString filename) {
    if (!text.isEmpty() && !text.endsWith('\n')) {
        text.append('\n');
    }
    QDir().mkpath(QFileInfo(filename).absolutePath());
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << text;
        file.close();
    }
}

static QString readTextFile(QString path) {
    QString result;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        result = in.readAll();
        file.close();
    }
    return result;
}

static qreal getEm(const QFont &font) {
    return QFontMetricsF(font).horizontalAdvance('M');
}

static void verticalMaximize(QMainWindow *window) {
    QScreen *screen = window->screen();
    if (screen != nullptr) {
        QRect u = window->geometry();
        QRect v = window->screen()->availableGeometry();
        window->setGeometry(u.x(), v.top(), u.width(), v.height());
    }
}

static void setFontSize(QWidget *widget, int pointSize) {
    QFont font = widget->font();
    font.setPointSize(pointSize);
    widget->setFont(font);
}

template <typename T>
static void connectKey(QKeySequence seq, T *receiver, void (T::*method)()) {
    static_assert(std::is_base_of<QWidget, T>::value);
    QObject::connect(new QShortcut(seq, receiver), &QShortcut::activated,
                     receiver, method);
}

static QString getTildePath(const QString &absolutePath) {
    QString cleanPath = QDir::cleanPath(absolutePath);
    QString homePath = QDir::homePath();
    if (cleanPath.startsWith(homePath + "/")) {
        cleanPath.replace(0, homePath.length(), "~");
    }
    return cleanPath;
}

// Parameter fmt should be a path relative to home directory, suitable for
// passing to QDate::toString.
static QString getJournalPath(const QString &fmt) {
    return QDir(QDir::homePath()).filePath(QDate::currentDate().toString(fmt));
}

////////////////////////////////////////////////////////////////////////////////

class SingleFileEditor : public QMainWindow {
    Q_OBJECT
    QString m_filename;
    QTextEdit *m_textEdit;
    QStatusBar *m_statusBar;
    QLabel *m_wordCount;
    int m_minimumColumns;

public:
    SingleFileEditor(const QString &filename, int fontsize, int minimumColumns);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void onTextChanged();
    void updateWordCount();
    void doClose() { this->close(); }
    void doSave();
    void doZoomIn();
    void doZoomOut();
};

SingleFileEditor::SingleFileEditor(const QString &filename, int fontsize,
                                   int minimumColumns)
    : QMainWindow(nullptr), m_filename(filename),
      m_textEdit(new QTextEdit(this)), m_statusBar(new QStatusBar(this)),
      m_wordCount(new QLabel(this)), m_minimumColumns(minimumColumns) {
    this->setWindowTitle("WordCountJournal: " + getTildePath(m_filename));

    m_textEdit->setAcceptRichText(false);
    m_textEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    setFontSize(m_textEdit, fontsize);
    setFontSize(this, fontsize);

    this->setCentralWidget(m_textEdit);
    this->setMinimumWidth((2 + m_minimumColumns) * getEm(m_textEdit->font()));

    m_statusBar->showMessage("saved");
    m_statusBar->addPermanentWidget(m_wordCount);
    this->setStatusBar(m_statusBar);

    m_textEdit->setPlainText(readTextFile(m_filename));
    this->updateWordCount();

    QObject::connect(m_textEdit, &QTextEdit::textChanged, this,
                     &SingleFileEditor::onTextChanged);

    connectKey(QKeySequence::Close, this, &SingleFileEditor::doClose);
    connectKey(QKeySequence::Quit, this, &SingleFileEditor::doClose);
    connectKey(QKeySequence::Save, this, &SingleFileEditor::doSave);
    connectKey(QKeySequence::ZoomIn, this, &SingleFileEditor::doZoomIn);
    connectKey(QKeySequence::ZoomOut, this, &SingleFileEditor::doZoomOut);
}

void SingleFileEditor::closeEvent(QCloseEvent *event) {
    saveTextFile(m_textEdit->toPlainText(), m_filename);
    QMainWindow::closeEvent(event);
}

void SingleFileEditor::onTextChanged() {
    this->updateWordCount();
    m_statusBar->showMessage("* unsaved *");
}

void SingleFileEditor::updateWordCount() {
    m_wordCount->setText(
        QString("Word Count: %1").arg(getWordCount(m_textEdit->document())));
}

void SingleFileEditor::doSave() {
    saveTextFile(m_textEdit->toPlainText(), m_filename);
    m_statusBar->showMessage("saved");
}

void SingleFileEditor::doZoomIn() {
    m_textEdit->zoomIn();
    this->setMinimumWidth((2 + m_minimumColumns) * getEm(m_textEdit->font()));
}

void SingleFileEditor::doZoomOut() {
    m_textEdit->zoomOut();
    this->setMinimumWidth((2 + m_minimumColumns) * getEm(m_textEdit->font()));
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("WordCountJournal");
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption sizeOption(QStringList() << "s" << "size", "Font Size.",
                                  "size", "12");
    parser.addOption(sizeOption);
    QCommandLineOption colOption(QStringList() << "c" << "columns",
                                 "Minimum Columns.", "columns", "80");
    parser.addOption(colOption);
    parser.process(app);

    bool conversionOk = false;
    int size = parser.value(sizeOption).toInt(&conversionOk);
    if (!conversionOk) {
        return 1;
    }
    int columns = parser.value(colOption).toInt(&conversionOk);
    if (!conversionOk) {
        return 1;
    }

    static const char fmt[] = "'Notes/'yyyy'/'MM'/notes-'yyyy-MM-dd_ddd'.txt'";
    SingleFileEditor editor(getJournalPath(fmt), size, columns);
    editor.show();
    QCoreApplication::processEvents();
    verticalMaximize(&editor);
    return app.exec();
}

#include "journal.moc"
