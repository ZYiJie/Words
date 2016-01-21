#include "mainwindow.h"
#include <chrono>
#include <random>
#include <functional>

#define FONT_NORMAL	24
#define FONT_LARGE	36

using std::chrono::system_clock;

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	unsigned seed = system_clock::now().time_since_epoch().count();
	generator = new std::default_random_engine(seed);
	wordCount = 0;
	QFont f(font());
	f.setPointSize(FONT_NORMAL);
	f.setFamily("Calibri");
	setFont(f);

	QWidget *w = new QWidget(this);
	setCentralWidget(w);
	QVBoxLayout *lay = new QVBoxLayout(w);
	QHBoxLayout *hlay;

	lay->addWidget(lLesson = new QLabel("Lesson *"));
	lay->addWidget(lKana = new QLabel("Kana"));
	lay->addWidget(lKanji = new QLabel("Kanji"));
	lay->addWidget(lEnglish = new QLabel("English"));
	f.setPointSize(FONT_NORMAL / 2);
	lLesson->setFont(f);
	f.setPointSize(FONT_LARGE);
	lEnglish->setFont(f);
	lEnglish->setWordWrap(true);
	f.setFamily("Yu Gothic");
	f.setPointSize(FONT_NORMAL);
	lKana->setFont(f);
	lKana->setWordWrap(true);
	lKanji->setFont(f);
	lKanji->setWordWrap(true);

	lay->addWidget(leInput = new QLineEdit);
	f.setPointSize(FONT_LARGE);
	leInput->setFont(f);

	lay->addLayout(hlay = new QHBoxLayout);
	hlay->addWidget(pbShow = new QPushButton(tr("&Show")));
	hlay->addWidget(pbLevel[0] = new QPushButton(tr("Yes (&Z)")));
	hlay->addWidget(pbLevel[1] = new QPushButton(tr("Skip (&X)")));
	hlay->addWidget(pbLevel[2] = new QPushButton(tr("No (&C)")));

	//lay->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Preferred, QSizePolicy::Expanding));
	//lay->addLayout(hlay = new QHBoxLayout);
	hlay->addWidget(pbOpen = new QPushButton(tr("&Open")));
	//hlay->addWidget(pbSave = new QPushButton(tr("S&ave")));
	//hlay->addWidget(pbNext = new QPushButton(tr("&Next")));

	hlay->addWidget(pbClear = new QPushButton(tr("Clea&r")));
	lay->addWidget(drawing = new Drawing(this));

	drawing->setColour(Qt::black);

	connect(pbOpen, SIGNAL(clicked(bool)), this, SLOT(openFile()));
	//connect(pbNext, SIGNAL(clicked(bool)), this, SLOT(newWord()));
	connect(pbShow, SIGNAL(clicked(bool)), this, SLOT(showWord()));
	connect(pbLevel[0], SIGNAL(clicked(bool)), this, SLOT(wordYes()));
	connect(pbLevel[1], SIGNAL(clicked(bool)), this, SLOT(newWord()));
	connect(pbLevel[2], SIGNAL(clicked(bool)), this, SLOT(wordNo()));
	connect(pbClear, SIGNAL(clicked(bool)), drawing, SLOT(clear()));
	connect(drawing, SIGNAL(active()), this, SLOT(drawingMode()));
	connect(leInput, SIGNAL(cursorPositionChanged(int,int)), this, SLOT(keyboardMode()));
}

void MainWindow::openFile()
{
	QString file = QFileDialog::getOpenFileName(this, QString(), QString(), tr("Json document (*.json)"));
	QFile f(file);
	if (!f.open(QFile::ReadWrite)) {
		QMessageBox::warning(this, tr("Error open file"), tr("Cannot open file for read"));
		return;
	}
	QJsonParseError error;
	QJsonDocument json(QJsonDocument::fromJson(f.readAll(), &error));
	if (error.error != QJsonParseError::NoError) {
		QMessageBox::warning(this, tr("Error load JSON"), tr("Error: %1\nOffset: %2").arg(error.errorString()).arg(error.offset));
		return;
	}
	if (!json.isObject()) {
		QMessageBox::warning(this, tr("Error load data"), tr("Invalid data format"));
		return;
	}

	int count = 0;
	QVector<lesson_t> lessons_new;
	std::vector<float> probabilities_new;
	QJsonObject obj(json.object());
	for (QJsonObject::Iterator it = obj.begin(); it != obj.end(); it++) {
		struct lesson_t lesson;
		lesson.name = it.key();
		QJsonArray array(it.value().toArray());
		lesson.count = array.count();
		lessons_new.append(lesson);
		count += lesson.count;

		foreach (QJsonValue val, array)
			probabilities_new.push_back(probability(val.toObject()));
	}
	if (!count) {
		QMessageBox::warning(this, tr("Error load data"), tr("Did not find any words"));
		return;
	}

	root = obj;
	lessons = lessons_new;
	probabilities = probabilities_new;
	wordCount = count;
	distribution = std::discrete_distribution<int>(probabilities.begin(), probabilities.end());
	debugProb();

	f.close();
	if (docFile.isOpen())
		docFile.close();
	docFile.setFileName(file);
	if (!docFile.open(QFile::ReadWrite)) {
		QMessageBox::warning(this, tr("Error open file"), tr("Cannot open file for write"));
		wordCount = 0;
		return;
	}

	newWord();
}

void MainWindow::newWord()
{
	if (!wordCount)
		return;

	int n = status.index = distribution(*generator);
	int lesson = 0;
	foreach (lesson_t l, lessons) {
		if (n < l.count)
			break;
		n -= l.count;
		lesson++;
	}

	QString lKey = root.keys().at(lesson);
	QJsonObject word(root.value(lKey).toArray().at(n).toObject());
	lKana->setVisible(false);
	lKanji->setVisible(false);
	lLesson->setText(lKey);
	lKana->setText(word.value("kana").toString());
	lKanji->setText(word.value("kanji").toString());
	lEnglish->setText(word.value("english").toString());
	pbLevel[0]->setText(tr("Yes/%1 (&Z)").arg(word["yes"].toInt()));
	pbLevel[2]->setText(tr("No/%1 (&C)").arg(word["no"].toInt()));
	leInput->clear();
	drawing->clear();
	drawing->setColour(Qt::black);
}

void MainWindow::showWord()
{
	lKana->setVisible(lKana->text().isEmpty() ? false : !lKana->isVisible());
	lKanji->setVisible(lKanji->text().isEmpty() ? false : !lKanji->isVisible());
	if (lKana->isVisible() || lKanji->isVisible())
		drawing->setColour(Qt::blue);
	else
		drawing->setColour(Qt::black);
}

void MainWindow::drawingMode()
{
	QFont f(leInput->font());
	f.setPointSize(FONT_NORMAL / 2);
	leInput->setFont(f);
}

void MainWindow::keyboardMode()
{
	QFont f(leInput->font());
	f.setPointSize(FONT_LARGE);
	leInput->setFont(f);
}

void MainWindow::wordParamInc(QString key, int inc)
{
	if (!wordCount)
		return;

	int n = status.index;
	int lesson = 0;
	foreach (lesson_t l, lessons) {
		if (n < l.count)
			break;
		n -= l.count;
		lesson++;
	}

	QString lKey = root.keys().at(lesson);
	QJsonArray array(root[lKey].toArray());
	QJsonObject word(array[n].toObject());
	word[key] = word[key].toInt() + inc;
	word["time"] = QDateTime::currentDateTime().toString(Qt::ISODate);
	array[n] = word;
	root[lKey] = array;

	probabilities[status.index] = probability(word);
	debugProb();

	newWord();
	distribution = std::discrete_distribution<int>(probabilities.begin(), probabilities.end());

	QByteArray json = QJsonDocument(root).toJson();
	docFile.seek(0);
	docFile.write(json);
	docFile.resize(json.size());
	docFile.flush();
}

void MainWindow::debugProb() const
{
	QMap<float, int> map;
	for (float p:probabilities)
		map[p] += 1;
	qDebug() << "Probabilities:" << map;
}

float MainWindow::probability(QJsonObject word)
{
	int yes = word["yes"].toInt();
	int no = word["no"].toInt();
	return no > yes ? 2.f - exp(yes - no) : exp(no - yes);
}