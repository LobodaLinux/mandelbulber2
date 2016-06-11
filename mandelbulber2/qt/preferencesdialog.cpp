#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include <QtCore>
#include <QFileDialog>
#include <QMessageBox>
#include "../src/file_downloader.hpp"
#include "../src/interface.hpp"
#include "../src/system.hpp"
#include "../src/initparameters.hpp"
#include "../src/settings.hpp"
#include "../src/global_data.hpp"

cPreferencesDialog::cPreferencesDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::cPreferencesDialog)
{
	initFinished = false;
	ui->setupUi(this);

	automatedWidgets = new cAutomatedWidgets(this);
	automatedWidgets->ConnectSignalsForSlidersInWindow(this);
	SynchronizeInterfaceWindow(this, gPar, qInterface::write);
	ui->comboBox_ui_style_type->addItems(QStyleFactory::keys());
	ui->comboBox_ui_style_type->setCurrentIndex(gPar->Get<int>("ui_style_type"));
	ui->comboBox_ui_skin->setCurrentIndex(gPar->Get<int>("ui_skin"));
	ui->comboboxLanguage->addItems(systemData.supportedLanguages.values());

	if(systemData.supportedLanguages.contains(gPar->Get<QString>("language")))
	{
		QString value = systemData.supportedLanguages[gPar->Get<QString>("language")];
		int index = ui->comboboxLanguage->findText(value);
		ui->comboboxLanguage->setCurrentIndex(index);
	}
	else
	{
		int index = ui->comboboxLanguage->findText(systemData.locale.name());
		ui->comboboxLanguage->setCurrentIndex(index);
	}
	initFinished = true;
}

cPreferencesDialog::~cPreferencesDialog()
{
  delete ui;
}

void cPreferencesDialog::on_buttonBox_accepted()
{
	SynchronizeInterfaceWindow(this, gPar, qInterface::read);

	QFont font = gMainInterface->mainWindow->font();
	font.setPixelSize(gPar->Get<int>("ui_font_size"));
	gMainInterface->mainWindow->setFont(font);

	QString value = ui->comboboxLanguage->currentText();
	QString key = systemData.supportedLanguages.key(value);
	gPar->Set<QString>("language", key);

	gPar->Set<int>("toolbar_icon_size", gPar->Get<int>("toolbar_icon_size"));
	gMainInterface->mainWindow->slotPopulateToolbar(true);

	systemData.loggingVerbosity = gPar->Get<int>("logging_verbosity");
}

void cPreferencesDialog::on_pushButton_select_image_path_clicked()
{
  QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, tr("Select default directory for images"), QDir::toNativeSeparators(ui->text_default_image_path->text())));
  if(dir.length() > 0)
  {
  	ui->text_default_image_path->setText(dir);
  }
}

void cPreferencesDialog::on_pushButton_select_settings_path_clicked()
{
  QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, tr("Select default directory for settings files"), QDir::toNativeSeparators(ui->text_default_settings_path->text())));
  if(dir.length() > 0)
  {
  	ui->text_default_settings_path->setText(dir);
  }
}

void cPreferencesDialog::on_pushButton_select_textures_path_clicked()
{
  QString dir = QDir::toNativeSeparators(QFileDialog::getExistingDirectory(this, tr("Select default directory for textures"), QDir::toNativeSeparators(ui->text_default_textures_path->text())));
  if(dir.length() > 0)
  {
  	ui->text_default_textures_path->setText(dir);
  }
}

void cPreferencesDialog::on_pushButton_clear_thumbnail_cache_clicked()
{
	QDir thumbnailDir(systemData.thumbnailDir);
	thumbnailDir.setFilter( QDir::AllEntries | QDir::NoDotAndDotDot );
	int thumbnailDirCount = thumbnailDir.count();

	//confirmation dialog before clearing
	QMessageBox::StandardButton reply;
	reply = QMessageBox::question(
		NULL,
		QObject::tr("Are you sure to clear the thumbnail cache?"),
		QObject::tr("Are you sure to clear the thumbnail cache?\nThere are currently %1 thumbnails cached. These will be deleted and rerendered when necessary.\n Clear now?").arg(thumbnailDirCount),
		QMessageBox::Yes|QMessageBox::No);

	if (reply == QMessageBox::Yes)
	{
		// match exact 32 char hash images, example filename: c0ad626d8c25ab6a25c8d19a53960c8a.png
		DeleteAllFilesFromDirectory(systemData.thumbnailDir, "????????????????????????????????.*");
	}
	else
	{
		return;
	}
}

void cPreferencesDialog::on_pushButton_load_thumbnail_cache_clicked()
{
	QMessageBox::StandardButton reply;
	reply = QMessageBox::question(
		NULL,
		QObject::tr("Are you sure to load the thumbnail cache from the server?"),
		QObject::tr("This will try to load missing common thumbnails from the server.\nProceed?"),
		QMessageBox::Yes|QMessageBox::No);

	if (reply == QMessageBox::Yes)
	{
		cFileDownloader fileDownloader(
					QString("http://cdn.mandelbulber.org/thumbnail/%1").arg(MANDELBULBER_VERSION_STRING),
					systemData.thumbnailDir
				);
		QObject::connect(&fileDownloader,
		 SIGNAL(updateProgressAndStatus(const QString&, const QString&, double)),
		 gMainInterface->mainWindow,
		 SLOT(slotUpdateProgressAndStatus(const QString&, const QString&, double)));
		fileDownloader.downloadFilelist();
	}
	else
	{
		return;
	}
}

void cPreferencesDialog::on_pushButton_generate_thumbnail_cache_clicked()
{
	QMessageBox::StandardButton reply;
	QString infoGenerateCacheFiles = "This is a development feature. If you want to obtain all example cache files, click load!\n\n";
	infoGenerateCacheFiles += "This will render all example files which are yet missing in your thumbnail cache.\n";
	infoGenerateCacheFiles += "This process will take a lot of time and cannot be cancelled.\nProceed?";

	reply = QMessageBox::question(
		NULL,
		QObject::tr("Are you sure to generate all example thumbnail cache files?"),
		infoGenerateCacheFiles,
		QMessageBox::Yes|QMessageBox::No);

	if (reply == QMessageBox::Yes)
	{
		// this renders all example files as a thumbnail and saves them to the thumbnail cache
		QString examplePath = QDir::toNativeSeparators(systemData.sharedDir + QDir::separator() + "examples");
		QDirIterator it(examplePath, QStringList() << "*.fract", QDir::Files, QDirIterator::Subdirectories);
		QStringList exampleFiles;
		while (it.hasNext()) exampleFiles << it.next();

		cParameterContainer* examplePar = new cParameterContainer;
		cFractalContainer* exampleParFractal = new cFractalContainer;
		cThumbnailWidget* thumbWidget = new cThumbnailWidget(200, 200, 1, this);
		QObject::connect(thumbWidget,
		 SIGNAL(updateProgressAndStatus(const QString&, const QString&, double)),
		 gMainInterface->mainWindow,
		 SLOT(slotUpdateProgressAndStatus(const QString&, const QString&, double)));

		examplePar->SetContainerName("main");
		InitParams(examplePar);
		/****************** TEMPORARY CODE FOR MATERIALS *******************/

		InitMaterialParams(1, examplePar);

		/*******************************************************************/
		for (int i = 0; i < NUMBER_OF_FRACTALS; i++)
		{
			exampleParFractal->at(i).SetContainerName(QString("fractal") + QString::number(i));
			InitFractalParams(&exampleParFractal->at(i));
		}
		for(int i = 0; i < exampleFiles.size(); i++)
		{
			QString filename = exampleFiles.at(i);
			gMainInterface->mainWindow->slotUpdateProgressAndStatus(
				QString("Rendering examples"), tr("rendering %1, %2 of %3").arg(
							filename,
							QString::number(i + 1),
							QString::number(exampleFiles.size())),
				1.0 * (i + 1) / exampleFiles.size(),
				cProgressText::progress_ANIMATION);

			cSettings parSettings(cSettings::formatFullText);
			parSettings.BeQuiet(true);
			parSettings.LoadFromFile(filename);

			if (parSettings.Decode(examplePar, exampleParFractal))
			{
				thumbWidget->AssignParameters(*examplePar, *exampleParFractal);
				if(!thumbWidget->IsRendered())
				{
					thumbWidget->slotRender();
					while(!thumbWidget->IsRendered())
					{
						gApplication->processEvents();
						Wait(100);
					}
				}
			}
		}
		delete exampleParFractal;
		delete examplePar;
		delete thumbWidget;
	}
	else
	{
		return;
	}
}

void cPreferencesDialog::on_comboBox_ui_style_type_currentIndexChanged(int index)
{
	if(!initFinished) return;
	gPar->Set<int>("ui_style_type", index);
	UpdateUIStyle();
}

void cPreferencesDialog::on_comboBox_ui_skin_currentIndexChanged(int index)
{
	if(!initFinished) return;
	gPar->Set<int>("ui_skin", index);
	UpdateUISkin();
}

void cPreferencesDialog::on_pushButton_retrieve_toolbar_clicked()
{
	QMessageBox::StandardButton reply;
	reply =
			QMessageBox::question(
			NULL,
			QObject::tr("Retrieve toolbar"), QObject::tr("Are you sure to retrieve default presets into toolbar?\nIt could overwrite some of existing presets.\nProceed?"),
			QMessageBox::Yes | QMessageBox::No);

	if (reply == QMessageBox::Yes)
	{
		RetrieveToolbarPresets(true);
	}
}

void cPreferencesDialog::on_pushButton_retrieve_materials_clicked()
{
	QMessageBox::StandardButton reply;
	reply =
			QMessageBox::question(
			NULL,
			QObject::tr("Retrieve materials"), QObject::tr("Are you sure to retrieve default materials into materials folder?\nIt could overwrite some of existing materials.\nProceed?"),
			QMessageBox::Yes | QMessageBox::No);

	if (reply == QMessageBox::Yes)
	{
		RetrieveExampleMaterials(true);
	}
}
