#include "FocalStackProcessor.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QTextStream>
#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <utility>
#include <vector>

bool FocalStackProcessor::loadFromFolder(const QString& folderPath, ProgressCallback onProgress)
{
    m_stack.clear();
    m_focalPowers.clear();

    QDir dir(folderPath);
    if (!dir.exists()) return false;

    const QStringList filters = {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tiff", "*.tif"};

    // ── Method 1: metadata.csv (explicit focal power map) ─────────────────
    QMap<QString, float> csvMap;
    QFile csvFile(dir.filePath("metadata.csv"));
    if (csvFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&csvFile);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;
            QStringList parts = line.split(',');
            if (parts.size() < 2) continue;
            QString fname = parts[0].trimmed();
            bool    ok    = false;
            float   power = parts[1].trimmed().toFloat(&ok);
            if (ok && !fname.isEmpty())
                csvMap[fname] = power;
        }
        csvFile.close();
    }

    // ── Build ordered file list and focal power vector ─────────────────────
    QStringList        files;
    std::vector<float> powers;

    if (!csvMap.isEmpty()) {
        // CSV present: load in CSV key order (QMap keys are alphabetically sorted)
        for (const QString& fname : csvMap.keys()) {
            files.append(fname);
            powers.push_back(csvMap[fname]);
        }
    } else {
        // ── Method 2: filename encodes the focal power (e.g. "-2.3.BMP") ──────
        // Strip the extension; if the result parses as a float, use it directly.
        QStringList allFiles = dir.entryList(filters, QDir::Files, QDir::Name);
        std::vector<std::pair<float, QString>> focalFiles;
        bool allParseable = !allFiles.isEmpty();

        for (const QString& fname : allFiles) {
            bool  ok    = false;
            float power = QFileInfo(fname).completeBaseName().toFloat(&ok);
            if (ok)
                focalFiles.push_back({power, fname});
            else
                allParseable = false;
        }

        if (allParseable && !focalFiles.empty()) {
            // Sort ascending by focal power so lowest diopter (lowest Z) comes first
            std::sort(focalFiles.begin(), focalFiles.end(),
                      [](const auto& a, const auto& b){ return a.first < b.first; });
            for (const auto& [power, fname] : focalFiles) {
                files.append(fname);
                powers.push_back(power);
            }
        } else {
            // ── Method 3: plain alphabetical, no focal metadata ────────────────
            files = allFiles;
        }
    }

    if (files.isEmpty()) return false;

    // ── Load images ────────────────────────────────────────────────────────
    int total = files.size();
    for (int i = 0; i < total; ++i) {
        cv::Mat img = cv::imread(dir.absoluteFilePath(files[i]).toStdString(),
                                 cv::IMREAD_GRAYSCALE);
        if (img.empty()) { m_stack.clear(); m_focalPowers.clear(); return false; }
        m_stack.push_back(std::move(img));

        if (onProgress)
            onProgress((i + 1) * 100 / total,
                       QString("Loading %1 / %2: %3").arg(i + 1).arg(total).arg(files[i]));
    }

    if (static_cast<int>(powers.size()) == total)
        m_focalPowers.assign(powers.begin(), powers.end());

    return true;
}
