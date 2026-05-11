#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QFile>
#include <QtCharts>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QDateTime>
#include <QFont>
#include <QFontDatabase>
#include <QShortcut>
#include <QKeySequence>
#include <QTimer>
#include <QSpinBox>
#include <QTime>
#include <QStringDecoder>
#include <cmath>

class StockViewWindow : public QMainWindow {
    Q_OBJECT

private:
    QTableWidget* table;
    QChartView* pieChartView;
    QChartView* lineChartView;
    QNetworkAccessManager* networkManager;
    QSqlDatabase db;
    
    QList<QMap<QString, QVariant>> holdings;
    QJsonArray marketData;
    QJsonArray tpexMarketData;
    QList<QMap<QString, QString>> fundData;
    
    QTimer* refreshTimer;
    int refreshIntervalSeconds;
    
    double zoomLevel;
    int baseFontSize;
    
    QStringList columns = {
        "Symbol", "Stock Name", "Current Price", "Price Change",
        "% Change", "# Shares", "Current Value", "Buying Price", "Cost",
        "Unrealized P&L", "% U P&L"
    };

public:
    StockViewWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("Portfolio Holdings Viewer");
        resize(1400, 900);
        
        zoomLevel = 1.3;
        baseFontSize = 11;
        refreshIntervalSeconds = 600;  // Default 60 seconds
        
        // Setup refresh timer
        refreshTimer = new QTimer(this);
        connect(refreshTimer, &QTimer::timeout, this, &StockViewWindow::autoRefresh);
        
        // Setup database
        setupDatabase();
        
        // Load Chinese font
        setupChineseFont();
        
        // Create central widget
        QWidget* centralWidget = new QWidget(this);
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
        
        // Create chart area
        QWidget* chartWidget = new QWidget();
        QHBoxLayout* chartLayout = new QHBoxLayout(chartWidget);
        
        pieChartView = new QChartView();
        pieChartView->setRenderHint(QPainter::Antialiasing);
        lineChartView = new QChartView();
        lineChartView->setRenderHint(QPainter::Antialiasing);
        
        chartLayout->addWidget(pieChartView);
        chartLayout->addWidget(lineChartView);
        
        mainLayout->addWidget(chartWidget, 1);
        
        // Create summary panel
        QWidget* summaryPanel = new QWidget();
        QVBoxLayout* summaryMainLayout = new QVBoxLayout(summaryPanel);
        summaryMainLayout->setContentsMargins(10, 5, 10, 5);
        summaryMainLayout->setSpacing(5);
        
        // First line: metrics
        QWidget* metricsWidget = new QWidget();
        QHBoxLayout* metricsLayout = new QHBoxLayout(metricsWidget);
        metricsLayout->setContentsMargins(0, 0, 0, 0);
        metricsLayout->setSpacing(30);
        
        QLabel* totalValueLabel = new QLabel("Total Value: $0.00");
        totalValueLabel->setStyleSheet("font-size: 14pt; font-weight: bold; color: #2196F3;");
        totalValueLabel->setObjectName("totalValueLabel");
        
        QLabel* totalCostLabel = new QLabel("Total Cost: $0.00");
        totalCostLabel->setStyleSheet("font-size: 14pt; font-weight: bold; color: #666;");
        totalCostLabel->setObjectName("totalCostLabel");
        
        QLabel* totalPLLabel = new QLabel("P&L: $0.00");
        totalPLLabel->setStyleSheet("font-size: 14pt; font-weight: bold;");
        totalPLLabel->setObjectName("totalPLLabel");
        
        QLabel* totalPctLabel = new QLabel("0.00%");
        totalPctLabel->setStyleSheet("font-size: 14pt; font-weight: bold;");
        totalPctLabel->setObjectName("totalPctLabel");
        
        QLabel* realizedPLLabel = new QLabel("Realized: $0.00");
        realizedPLLabel->setStyleSheet("font-size: 14pt; font-weight: bold; color: #FF9800;");
        realizedPLLabel->setObjectName("realizedPLLabel");
        
        metricsLayout->addWidget(totalValueLabel);
        metricsLayout->addWidget(totalCostLabel);
        metricsLayout->addWidget(totalPLLabel);
        metricsLayout->addWidget(totalPctLabel);
        metricsLayout->addWidget(realizedPLLabel);
        metricsLayout->addStretch();
        
        // Second line: warning
        QLabel* WarnLabel = new QLabel("All investments involve risk, including possible loss of principal. Investors should consider the investment objectives, risks, charges and expenses carefully before investing.");
        WarnLabel->setStyleSheet("font-size: 10pt; font-weight: bold; color: #ffffff;");
        WarnLabel->setObjectName("warnLabel");
        
        summaryMainLayout->addWidget(metricsWidget);
        summaryMainLayout->addWidget(WarnLabel);
        
        mainLayout->addWidget(summaryPanel);
        
        // Create table
        table = new QTableWidget();
        table->setColumnCount(11);
        table->setHorizontalHeaderLabels({
            "Symbol", "Name", "Price", "Change", "%Chg", 
            "Shares", "Value", "Cost/Sh", "Cost", "P&L", "%P&L"
        });
        table->horizontalHeader()->setStretchLastSection(false);
        table->setColumnWidth(0, 80);  // Symbol
        table->setColumnWidth(1, 120); // Name
        table->setColumnWidth(2, 70);  // Price
        table->setColumnWidth(3, 60);  // Change
        table->setColumnWidth(4, 60);  // %Chg
        table->setColumnWidth(5, 70);  // Shares
        table->setColumnWidth(6, 100); // Value
        table->setColumnWidth(7, 70);  // Cost/Sh
        table->setColumnWidth(8, 100); // Cost
        table->setColumnWidth(9, 100); // P&L
        table->setColumnWidth(10, 70); // %P&L
        table->setAlternatingRowColors(true);
        table->setSortingEnabled(true);
        
        mainLayout->addWidget(table, 2);
        
        // Create button panel
        QWidget* buttonPanel = new QWidget();
        QHBoxLayout* buttonLayout = new QHBoxLayout(buttonPanel);
        
        QPushButton* refreshBtn = new QPushButton("Refresh Data");
        QPushButton* updateBtn = new QPushButton("Update && Record");
        QPushButton* addBtn = new QPushButton("Add Holding");
        QPushButton* closeBtn = new QPushButton("Close Position");
        
        // Auto-refresh controls
        QLabel* refreshLabel = new QLabel("  Auto-Refresh (seconds):");
        QSpinBox* intervalSpinBox = new QSpinBox();
        intervalSpinBox->setRange(5, 600);
        intervalSpinBox->setValue(60);
        intervalSpinBox->setSuffix(" sec");
        intervalSpinBox->setMaximumWidth(100);
        
        QPushButton* startRefreshBtn = new QPushButton("Start");
        QPushButton* stopRefreshBtn = new QPushButton("Stop");
        startRefreshBtn->setMaximumWidth(60);
        stopRefreshBtn->setMaximumWidth(60);
        stopRefreshBtn->setEnabled(false);
        
        QLabel* zoomLabel = new QLabel("  Table Zoom:");
        QPushButton* zoomInBtn = new QPushButton("+");
        QPushButton* zoomOutBtn = new QPushButton("-");
        QPushButton* zoomResetBtn = new QPushButton("Reset");
        
        zoomInBtn->setMaximumWidth(30);
        zoomOutBtn->setMaximumWidth(30);
        zoomResetBtn->setMaximumWidth(60);
        
        buttonLayout->addWidget(refreshBtn);
        buttonLayout->addWidget(updateBtn);
        buttonLayout->addWidget(addBtn);
        buttonLayout->addWidget(closeBtn);
        buttonLayout->addWidget(refreshLabel);
        buttonLayout->addWidget(intervalSpinBox);
        buttonLayout->addWidget(startRefreshBtn);
        buttonLayout->addWidget(stopRefreshBtn);
        buttonLayout->addWidget(zoomLabel);
        buttonLayout->addWidget(zoomInBtn);
        buttonLayout->addWidget(zoomOutBtn);
        buttonLayout->addWidget(zoomResetBtn);
        
        QPushButton* periodicInvestBtn = new QPushButton("Periodic Investment");
        connect(periodicInvestBtn, &QPushButton::clicked, this, &StockViewWindow::showPeriodicInvestmentDialog);
        buttonLayout->addWidget(periodicInvestBtn);
        
        buttonLayout->addStretch();
        
        mainLayout->addWidget(buttonPanel);
        
        setCentralWidget(centralWidget);
        
        // Setup network manager
        networkManager = new QNetworkAccessManager(this);
        
        // Connect signals
        connect(refreshBtn, &QPushButton::clicked, this, &StockViewWindow::loadData);
        connect(updateBtn, &QPushButton::clicked, this, &StockViewWindow::updateAndRecord);
        connect(addBtn, &QPushButton::clicked, this, &StockViewWindow::addHolding);
        connect(closeBtn, &QPushButton::clicked, this, &StockViewWindow::closePosition);
        connect(intervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
            refreshIntervalSeconds = value;
            if (refreshTimer->isActive()) {
                refreshTimer->setInterval(value * 1000);
            }
        });
        connect(startRefreshBtn, &QPushButton::clicked, [this, startRefreshBtn, stopRefreshBtn]() {
            if (isMarketHours()) {
                refreshTimer->start(refreshIntervalSeconds * 1000);
                startRefreshBtn->setEnabled(false);
                stopRefreshBtn->setEnabled(true);
                QMessageBox::information(this, "Auto-Refresh", "Auto-refresh started during market hours (08:30-13:30 GMT)");
            } else {
                QMessageBox::warning(this, "Market Closed", "Auto-refresh only works during market hours (08:30-13:30 GMT)\nCurrent time: " + 
                                    QDateTime::currentDateTimeUtc().toString("hh:mm:ss") + " GMT");
            }
        });
        connect(stopRefreshBtn, &QPushButton::clicked, [this, startRefreshBtn, stopRefreshBtn]() {
            refreshTimer->stop();
            startRefreshBtn->setEnabled(true);
            stopRefreshBtn->setEnabled(false);
        });
        connect(zoomInBtn, &QPushButton::clicked, this, &StockViewWindow::zoomIn);
        connect(zoomOutBtn, &QPushButton::clicked, this, &StockViewWindow::zoomOut);
        connect(zoomResetBtn, &QPushButton::clicked, this, &StockViewWindow::zoomReset);
        
        // Setup keyboard shortcuts
        QShortcut* zoomInShortcut1 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus), this);
        connect(zoomInShortcut1, &QShortcut::activated, this, &StockViewWindow::zoomIn);
        
        QShortcut* zoomInShortcut2 = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Equal), this);
        connect(zoomInShortcut2, &QShortcut::activated, this, &StockViewWindow::zoomIn);
        
        QShortcut* zoomOutShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus), this);
        connect(zoomOutShortcut, &QShortcut::activated, this, &StockViewWindow::zoomOut);
        
        QShortcut* zoomResetShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this);
        connect(zoomResetShortcut, &QShortcut::activated, this, &StockViewWindow::zoomReset);
        
        // Load initial data
        loadHoldings();
        loadFundData();
        loadData();
        applyZoom();
        
        // Periodic investment check timer (check every hour)
        QTimer* investTimer = new QTimer(this);
        connect(investTimer, &QTimer::timeout, this, &StockViewWindow::checkPeriodicInvestments);
        investTimer->start(3600000); // Check every hour
        checkPeriodicInvestments(); // Check on startup
    }

private slots:
    void setupDatabase() {
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName("/home/impartialjust/project/stockview/sqlite3.db");
        
        if (!db.open()) {
            QMessageBox::critical(this, "Error", "Failed to open database: " + db.lastError().text());
            return;
        }
        
        // Create PeriodicInvestment table if it doesn't exist
        QSqlQuery query(db);
        query.exec("CREATE TABLE IF NOT EXISTS PeriodicInvestment ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                   "symbol TEXT NOT NULL, "
                   "amount REAL NOT NULL, "
                   "interval_days INTEGER NOT NULL, "
                   "next_date INTEGER NOT NULL, "
                   "enabled INTEGER DEFAULT 1)");
        
        // Create RealizedPL table for closed positions
        query.exec("CREATE TABLE IF NOT EXISTS RealizedPL ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                   "symbol TEXT NOT NULL, "
                   "shares REAL NOT NULL, "
                   "buying_price REAL NOT NULL, "
                   "selling_price REAL NOT NULL, "
                   "realized_pl REAL NOT NULL, "
                   "close_date INTEGER NOT NULL)");
    }
    
    void setupChineseFont() {
        QString fontPath = "NSTC.ttf";
        if (QFile::exists(fontPath)) {
            int fontId = QFontDatabase::addApplicationFont(fontPath);
            if (fontId != -1) {
                QStringList fontFamilies = QFontDatabase::applicationFontFamilies(fontId);
                if (!fontFamilies.isEmpty()) {
                    QFont font(fontFamilies.at(0));
                    qApp->setFont(font);
                }
            }
        }
    }
    
    void loadHoldings() {
        holdings.clear();
        
        QSqlQuery query(db);
        if (query.exec("SELECT Symbol, Shares, CostPerShare, Date, uid FROM Holding")) {
            while (query.next()) {
                QMap<QString, QVariant> holding;
                holding["symbol"] = query.value(0).toString();
                holding["shares"] = query.value(1).toDouble();
                holding["buying_price"] = query.value(2).toDouble();
                holding["date"] = query.value(3).toLongLong();
                holding["uid"] = query.value(4).toInt();
                holding["exchange"] = "TWSE";
                holdings.append(holding);
            }
        }
    }
    
    void loadData() {
        QNetworkRequest request(QUrl("https://openapi.twse.com.tw/v1/exchangeReport/STOCK_DAY_ALL"));
        request.setRawHeader("accept", "application/json");
        request.setRawHeader("If-Modified-Since", "Mon, 26 Jul 1997 05:00:00 GMT");
        request.setRawHeader("Cache-Control", "no-cache");
        request.setRawHeader("Pragma", "no-cache");
        
        QNetworkReply* reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &StockViewWindow::onDataReceived);
    }
    
    void loadTpexData() {
        QNetworkRequest request(QUrl("https://www.tpex.org.tw/openapi/v1/tpex_mainboard_daily_close_quotes"));
        request.setRawHeader("accept", "application/json");
        request.setRawHeader("If-Modified-Since", "Mon, 26 Jul 1997 05:00:00 GMT");
        request.setRawHeader("Cache-Control", "no-cache");
        request.setRawHeader("Pragma", "no-cache");
        
        QNetworkReply* reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &StockViewWindow::onTpexDataReceived);
    }
    
    void loadFundData() {
        QNetworkRequest request(QUrl("https://www.sitca.org.tw/MemberK0000/F/03/nav.csv"));
        request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36");
        request.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,text/csv,*/*;q=0.8");
        
        QNetworkReply* reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, &StockViewWindow::onFundDataReceived);
    }
    
    void onFundDataReceived() {
        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply) return;
        
        fundData.clear();
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            
            // Save to file first
            QString filePath = "nav.csv";
            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
            }
            
            // Parse CSV (assuming Big5 encoding, common for Taiwan websites)
            auto decoder = QStringDecoder(QStringDecoder::System);
            QString csvText = decoder.decode(data);
            
            QStringList lines = csvText.split('\n');
            
            // Skip header line
            for (int i = 1; i < lines.size(); i++) {
                QString line = lines[i].trimmed();
                if (line.isEmpty()) continue;
                
                QStringList fields = line.split(',');
                if (fields.size() >= 7) {
                    QMap<QString, QString> fund;
                    fund["code"] = fields[3].trimmed();  // 基金統編 (column 4)
                    fund["name"] = fields[5].trimmed();  // 基金名稱 (column 6)
                    fund["nav"] = fields[6].trimmed();   // 基金淨值 (column 7)
                    fundData.append(fund);
                }
            }
            
            
            updateTable();
            updateCharts();
        } else {
            qDebug() << "Failed to fetch fund data:" << reply->errorString();
            // Still update table with stock data
            updateTable();
            updateCharts();
        }
        
        reply->deleteLater();
    }
    
    void onDataReceived() {
        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply) return;
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            marketData = doc.array();
            
            // Reload holdings and fund data from database/network
            loadHoldings();
            loadTpexData();
            loadFundData();
        } else {
            QMessageBox::warning(this, "Warning", "Could not fetch TWSE market data");
        }
        
        reply->deleteLater();
    }
    
    void onTpexDataReceived() {
        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        if (!reply) return;
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            tpexMarketData = doc.array();
        } else {
            qDebug() << "Could not fetch TPEX market data:" << reply->errorString();
        }
        
        reply->deleteLater();
    }
    
    void updateTable() {
        table->setRowCount(0);
        
        QMap<QString, QJsonObject> stockDict;
        for (const QJsonValue& val : marketData) {
            QJsonObject stock = val.toObject();
            stockDict[stock["Code"].toString()] = stock;
        }
        
        // Add TPEX stocks
        for (const QJsonValue& val : tpexMarketData) {
            QJsonObject stock = val.toObject();
            QString code = stock["SecuritiesCompanyCode"].toString();
            if (!code.isEmpty()) {
                stockDict[code] = stock;
            }
        }
        
        QMap<QString, QMap<QString, QString>> fundDict;
        for (const auto& fund : fundData) {
            fundDict[fund["code"]] = fund;
        }
        
        // Calculate totals
        double totalValue = 0;
        double totalCost = 0;
        
        for (const auto& holding : holdings) {
            QString symbol = holding["symbol"].toString();
            double shares = holding["shares"].toDouble();
            double buyingPrice = holding["buying_price"].toDouble();
            
            
            // Check if it's a stock
            if (stockDict.contains(symbol)) {
                QJsonObject stockInfo = stockDict[symbol];
                QString stockName = stockInfo["Name"].toString();
                if (stockName.isEmpty()) {
                    stockName = stockInfo["CompanyName"].toString(); // TPEX uses CompanyName
                }
                double currentPrice = parseNumber(stockInfo["ClosingPrice"]);
                if (currentPrice == 0) {
                    currentPrice = parseNumber(stockInfo["Close"]); // TPEX uses Close
                }
                double change = parseNumber(stockInfo["Change"]);
                if (change == 0 && currentPrice > 0) {
                    // TPEX doesn't have Change, calculate it
                    change = 0; // We'll leave it as 0 for TPEX stocks
                }
                
                double pctChange = (currentPrice - change != 0) ? 
                    (change / (currentPrice - change)) * 100 : 0;
                
                double currentValue = currentPrice * shares;
                double cost = buyingPrice * shares;
                double unrealizedPL = currentValue - cost;
                double pctUPL = (cost != 0) ? (unrealizedPL / cost * 100) : 0;
                
                totalValue += currentValue;
                totalCost += cost;
                
                int row = table->rowCount();
                table->insertRow(row);
                
                table->setItem(row, 0, new QTableWidgetItem(symbol));
                table->setItem(row, 1, new QTableWidgetItem(stockName));
                table->setItem(row, 2, new QTableWidgetItem(QString::number(currentPrice, 'f', 2)));
                table->setItem(row, 3, new QTableWidgetItem(QString("%1%2").arg(change >= 0 ? "+" : "").arg(change, 0, 'f', 2)));
                table->setItem(row, 4, new QTableWidgetItem(QString("%1%2%").arg(pctChange >= 0 ? "+" : "").arg(pctChange, 0, 'f', 2)));
                table->setItem(row, 5, new QTableWidgetItem(QString::number(shares, 'f', 2))); // Show 2 decimal places
                table->setItem(row, 6, new QTableWidgetItem(QString::number(currentValue, 'f', 2)));
                table->setItem(row, 7, new QTableWidgetItem(QString::number(buyingPrice, 'f', 2)));
                table->setItem(row, 8, new QTableWidgetItem(QString::number(cost, 'f', 2)));
                table->setItem(row, 9, new QTableWidgetItem(QString("%1%2").arg(unrealizedPL >= 0 ? "+" : "").arg(unrealizedPL, 0, 'f', 2)));
                table->setItem(row, 10, new QTableWidgetItem(QString("%1%2%").arg(pctUPL >= 0 ? "+" : "").arg(pctUPL, 0, 'f', 2)));
                
                // Color coding
                QColor color = (unrealizedPL > 0) ? QColor(40, 167, 69) : 
                               (unrealizedPL < 0) ? QColor(220, 53, 69) : QColor(108, 117, 125);
                
                for (int col = 0; col < 11; col++) {
                    table->item(row, col)->setForeground(QBrush(color));
                }
            }
            // Check if it's a fund
            else if (fundDict.contains(symbol)) {
                QMap<QString, QString> fundInfo = fundDict[symbol];
                QString fundName = fundInfo["name"];
                double currentPrice = fundInfo["nav"].toDouble();
                
                double currentValue = currentPrice * shares;
                double cost = buyingPrice * shares;
                double unrealizedPL = currentValue - cost;
                double pctUPL = (cost != 0) ? (unrealizedPL / cost * 100) : 0;
                double change = currentPrice - buyingPrice;
                double pctChange = (buyingPrice != 0) ? (change / buyingPrice * 100) : 0;
                
                totalValue += currentValue;
                totalCost += cost;
                
                int row = table->rowCount();
                table->insertRow(row);
                
                table->setItem(row, 0, new QTableWidgetItem(symbol + " (Fund)"));
                table->setItem(row, 1, new QTableWidgetItem(fundName));
                table->setItem(row, 2, new QTableWidgetItem(QString::number(currentPrice, 'f', 2)));
                table->setItem(row, 3, new QTableWidgetItem(QString("%1%2").arg(change >= 0 ? "+" : "").arg(change, 0, 'f', 2)));
                table->setItem(row, 4, new QTableWidgetItem(QString("%1%2%").arg(pctChange >= 0 ? "+" : "").arg(pctChange, 0, 'f', 2)));
                table->setItem(row, 5, new QTableWidgetItem(QString::number(shares, 'f', 2))); // Show 2 decimal places
                table->setItem(row, 6, new QTableWidgetItem(QString::number(currentValue, 'f', 2)));
                table->setItem(row, 7, new QTableWidgetItem(QString::number(buyingPrice, 'f', 2)));
                table->setItem(row, 8, new QTableWidgetItem(QString::number(cost, 'f', 2)));
                table->setItem(row, 9, new QTableWidgetItem(QString("%1%2").arg(unrealizedPL >= 0 ? "+" : "").arg(unrealizedPL, 0, 'f', 2)));
                table->setItem(row, 10, new QTableWidgetItem(QString("%1%2%").arg(pctUPL >= 0 ? "+" : "").arg(pctUPL, 0, 'f', 2)));
                
                // Color coding
                QColor color = (unrealizedPL > 0) ? QColor(40, 167, 69) : 
                               (unrealizedPL < 0) ? QColor(220, 53, 69) : QColor(108, 117, 125);
                
                for (int col = 0; col < 11; col++) {
                    table->item(row, col)->setForeground(QBrush(color));
                }
            }
        }
        
        // Update summary labels
        double totalPL = totalValue - totalCost;
        double totalPct = (totalCost != 0) ? (totalPL / totalCost * 100) : 0;
        
        // Calculate total realized P&L
        double totalRealizedPL = 0;
        QSqlQuery realizedQuery(db);
        if (realizedQuery.exec("SELECT SUM(realized_pl) FROM RealizedPL")) {
            if (realizedQuery.next()) {
                totalRealizedPL = realizedQuery.value(0).toDouble();
            }
        }
        
        QLabel* totalValueLabel = findChild<QLabel*>("totalValueLabel");
        QLabel* totalCostLabel = findChild<QLabel*>("totalCostLabel");
        QLabel* totalPLLabel = findChild<QLabel*>("totalPLLabel");
        QLabel* totalPctLabel = findChild<QLabel*>("totalPctLabel");
        QLabel* realizedPLLabel = findChild<QLabel*>("realizedPLLabel");
        
        if (totalValueLabel) totalValueLabel->setText(QString("Total Value: $%1").arg(totalValue, 0, 'f', 2));
        if (totalCostLabel) totalCostLabel->setText(QString("Total Cost: $%1").arg(totalCost, 0, 'f', 2));
        
        if (totalPLLabel) {
            QString plColor = (totalPL > 0) ? "#28a745" : (totalPL < 0) ? "#dc3545" : "#666";
            totalPLLabel->setStyleSheet(QString("font-size: 14pt; font-weight: bold; color: %1;").arg(plColor));
            totalPLLabel->setText(QString("P&L: $%1%2").arg(totalPL >= 0 ? "+" : "").arg(totalPL, 0, 'f', 2));
        }
        
        if (totalPctLabel) {
            QString pctColor = (totalPct > 0) ? "#28a745" : (totalPct < 0) ? "#dc3545" : "#666";
            totalPctLabel->setStyleSheet(QString("font-size: 14pt; font-weight: bold; color: %1;").arg(pctColor));
            totalPctLabel->setText(QString("%1%2%").arg(totalPct >= 0 ? "+" : "").arg(totalPct, 0, 'f', 2));
        }
        
        if (realizedPLLabel) {
            QString realizedColor = (totalRealizedPL > 0) ? "#28a745" : (totalRealizedPL < 0) ? "#dc3545" : "#FF9800";
            realizedPLLabel->setStyleSheet(QString("font-size: 14pt; font-weight: bold; color: %1;").arg(realizedColor));
            realizedPLLabel->setText(QString("Realized: $%1%2").arg(totalRealizedPL >= 0 ? "+" : "").arg(totalRealizedPL, 0, 'f', 2));
        }
        
        table->resizeColumnsToContents();
    }
    
    void updateCharts() {
        updatePieChart();
        updateLineChart();
    }
    
    void updatePieChart() {
        QMap<QString, QJsonObject> stockDict;
        for (const QJsonValue& val : marketData) {
            QJsonObject stock = val.toObject();
            stockDict[stock["Code"].toString()] = stock;
        }
        
        // Add TPEX stocks
        for (const QJsonValue& val : tpexMarketData) {
            QJsonObject stock = val.toObject();
            QString code = stock["SecuritiesCompanyCode"].toString();
            if (!code.isEmpty()) {
                stockDict[code] = stock;
            }
        }
        
        QMap<QString, QMap<QString, QString>> fundDict;
        for (const auto& fund : fundData) {
            fundDict[fund["code"]] = fund;
        }
        
        // Group holdings by symbol
        QMap<QString, QList<QMap<QString, QVariant>>> groupedHoldings;
        for (const auto& holding : holdings) {
            QString symbol = holding["symbol"].toString();
            groupedHoldings[symbol].append(holding);
        }
        
        QPieSeries* series = new QPieSeries();
        double totalValue = 0;
        
        // Generate colors based on unique symbols
        QList<QColor> colors = generateColors(groupedHoldings.size());
        QMap<QString, QColor> symbolColors;
        int colorIndex = 0;
        for (const QString& symbol : groupedHoldings.keys()) {
            symbolColors[symbol] = colors[colorIndex++];
        }
        
        // Process each unique symbol
        for (const QString& symbol : groupedHoldings.keys()) {
            const QList<QMap<QString, QVariant>>& symbolHoldings = groupedHoldings[symbol];
            
            // Aggregate shares and calculate total values
            double totalShares = 0;
            double totalCost = 0;
            for (const auto& holding : symbolHoldings) {
                double shares = holding["shares"].toDouble();
                double buyingPrice = holding["buying_price"].toDouble();
                totalShares += shares;
                totalCost += buyingPrice * shares;
            }
            
            // Check if it's a stock
            if (stockDict.contains(symbol)) {
                QJsonObject stockInfo = stockDict[symbol];
                double currentPrice = parseNumber(stockInfo["ClosingPrice"]);
                if (currentPrice == 0) {
                    currentPrice = parseNumber(stockInfo["Close"]); // TPEX uses Close
                }
                
                if (currentPrice > 0) {
                    double currentValue = currentPrice * totalShares;
                    double unrealizedPL = currentValue - totalCost;
                    double pctUPL = (totalCost != 0) ? (unrealizedPL / totalCost * 100) : 0;
                    QString stockName = stockInfo["Name"].toString();
                    if (stockName.isEmpty()) {
                        stockName = stockInfo["CompanyName"].toString(); // TPEX uses CompanyName
                    }
                    
                    QPieSlice* slice = series->append(symbol + "\n" + stockName.left(8), currentValue);
                    slice->setColor(symbolColors[symbol]);
                    slice->setLabelVisible(true);
                    
                    // Create detailed tooltip
                    QString tooltip = QString("%1 - %2\n"
                                             "Shares: %3\n"
                                             "Value: $%4\n"
                                             "Cost: $%5\n"
                                             "P&L: $%6 (%7%)")
                                      .arg(symbol)
                                      .arg(stockName)
                                      .arg(totalShares, 0, 'f', 2)
                                      .arg(currentValue, 0, 'f', 2)
                                      .arg(totalCost, 0, 'f', 2)
                                      .arg(unrealizedPL, 0, 'f', 2)
                                      .arg(pctUPL, 0, 'f', 2);
                    slice->setLabel(QString("%1\n%2%").arg(symbol + " - " + stockName.left(8)).arg(slice->percentage() * 100, 0, 'f', 1));
                    
                    // Enable hover effects
                    slice->setExplodeDistanceFactor(0.05);
                    connect(slice, &QPieSlice::hovered, [slice](bool state) {
                        slice->setExploded(state);
                        if (state) {
                            slice->setLabelFont(QFont("Arial", 12, QFont::Bold));
                        } else {
                            slice->setLabelFont(QFont("Arial", 9));
                        }
                    });
                    
                    totalValue += currentValue;
                }
            }
            // Check if it's a fund
            else if (fundDict.contains(symbol)) {
                QMap<QString, QString> fundInfo = fundDict[symbol];
                double currentPrice = fundInfo["nav"].toDouble();
                QString fundName = fundInfo["name"];
                
                if (currentPrice > 0) {
                    double currentValue = currentPrice * totalShares;
                    double unrealizedPL = currentValue - totalCost;
                    double pctUPL = (totalCost != 0) ? (unrealizedPL / totalCost * 100) : 0;
                    
                    QPieSlice* slice = series->append(symbol + " (Fund)\n" + fundName.left(8), currentValue);
                    slice->setColor(symbolColors[symbol]);
                    slice->setLabelVisible(true);
                    
                    QString tooltip = QString("%1 - %2\n"
                                             "Shares: %3\n"
                                             "Value: $%4\n"
                                             "Cost: $%5\n"
                                             "P&L: $%6 (%7%)")
                                      .arg(symbol)
                                      .arg(fundName)
                                      .arg(totalShares, 0, 'f', 2)
                                      .arg(currentValue, 0, 'f', 2)
                                      .arg(totalCost, 0, 'f', 2)
                                      .arg(unrealizedPL, 0, 'f', 2)
                                      .arg(pctUPL, 0, 'f', 2);
                    slice->setLabel(QString("%1\n%2%").arg(symbol + " - " + fundName.left(8)).arg(slice->percentage() * 100, 0, 'f', 1));
                    
                    slice->setExplodeDistanceFactor(0.05);
                    connect(slice, &QPieSlice::hovered, [slice](bool state) {
                        slice->setExploded(state);
                        if (state) {
                            slice->setLabelFont(QFont("Arial", 12, QFont::Bold));
                        } else {
                            slice->setLabelFont(QFont("Arial", 9));
                        }
                    });
                    
                    totalValue += currentValue;
                }
            }
        }
        
        QChart* chart = new QChart();
        chart->addSeries(series);
        chart->setTitle(QString("Portfolio Distribution - Total: $%1").arg(totalValue, 0, 'f', 2));
        chart->legend()->hide();
        chart->setAnimationOptions(QChart::SeriesAnimations);
        
        pieChartView->setChart(chart);
        pieChartView->setRenderHint(QPainter::Antialiasing);
    }
    
    void updateLineChart() {
        QSqlQuery query(db);
        
        // Create history table if needed
        query.exec("CREATE TABLE IF NOT EXISTS PortfolioHistory (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER NOT NULL, total_value REAL NOT NULL, total_pl REAL NOT NULL)");
        
        if (query.exec("SELECT timestamp, total_value, total_pl FROM PortfolioHistory ORDER BY timestamp ASC")) {
            QLineSeries* valueSeries = new QLineSeries();
            valueSeries->setName("Total Value");
            
            QLineSeries* plSeries = new QLineSeries();
            plSeries->setName("Total P&L");
            
            bool hasData = false;
            while (query.next()) {
                qint64 timestamp = query.value(0).toLongLong();
                double totalValue = query.value(1).toDouble();
                double totalPL = query.value(2).toDouble();
                
                valueSeries->append(timestamp * 1000, totalValue);
                plSeries->append(timestamp * 1000, totalPL);
                hasData = true;
            }
            
            QChart* chart = new QChart();
            
            if (hasData) {
                chart->addSeries(valueSeries);
                chart->addSeries(plSeries);
                
                QDateTimeAxis* axisX = new QDateTimeAxis();
                axisX->setFormat("MM/dd");
                axisX->setTitleText("Date");
                chart->addAxis(axisX, Qt::AlignBottom);
                valueSeries->attachAxis(axisX);
                plSeries->attachAxis(axisX);
                
                QValueAxis* axisY = new QValueAxis();
                axisY->setTitleText("Value ($)");
                chart->addAxis(axisY, Qt::AlignLeft);
                valueSeries->attachAxis(axisY);
                plSeries->attachAxis(axisY);
                
                chart->legend()->setVisible(true);
            } else {
                chart->setTitle("No historical data yet\nClick 'Update & Record' to start tracking");
            }
            
            chart->setTitle("Portfolio History");
            lineChartView->setChart(chart);
        }
    }
    
    void updateAndRecord() {
        QMap<QString, QJsonObject> stockDict;
        for (const QJsonValue& val : marketData) {
            QJsonObject stock = val.toObject();
            stockDict[stock["Code"].toString()] = stock;
        }
        
        // Add TPEX stocks
        for (const QJsonValue& val : tpexMarketData) {
            QJsonObject stock = val.toObject();
            QString code = stock["SecuritiesCompanyCode"].toString();
            if (!code.isEmpty()) {
                stockDict[code] = stock;
            }
        }
        
        double totalValue = 0;
        double totalPL = 0;
        
        for (const auto& holding : holdings) {
            QString symbol = holding["symbol"].toString();
            int shares = holding["shares"].toInt();
            double buyingPrice = holding["buying_price"].toDouble();
            
            if (stockDict.contains(symbol)) {
                double currentPrice = parseNumber(stockDict[symbol]["ClosingPrice"]);
                if (currentPrice == 0) {
                    currentPrice = parseNumber(stockDict[symbol]["Close"]); // TPEX uses Close
                }
                if (currentPrice > 0) {
                    double currentValue = currentPrice * shares;
                    double cost = buyingPrice * shares;
                    totalValue += currentValue;
                    totalPL += (currentValue - cost);
                }
            }
        }
        
        // Save to history
        QSqlQuery query(db);
        query.prepare("INSERT INTO PortfolioHistory (timestamp, total_value, total_pl) VALUES (?, ?, ?)");
        query.addBindValue(QDateTime::currentSecsSinceEpoch());
        query.addBindValue(totalValue);
        query.addBindValue(totalPL);
        query.exec();
        
        loadData();
        
        QMessageBox::information(this, "Success", 
            QString("Portfolio data recorded!\nTotal Value: $%1\nTotal P&L: $%2")
            .arg(totalValue, 0, 'f', 2)
            .arg(totalPL, 0, 'f', 2));
    }
    
    void addHolding() {
        QDialog dialog(this);
        dialog.setWindowTitle("Add Holding");
        
        QFormLayout layout(&dialog);
        
        QLineEdit* symbolEdit = new QLineEdit();
        QLineEdit* sharesEdit = new QLineEdit();
        QLineEdit* priceEdit = new QLineEdit();
        QLineEdit* exchangeEdit = new QLineEdit("TWSE");
        
        layout.addRow("Symbol:", symbolEdit);
        layout.addRow("Shares:", sharesEdit);
        layout.addRow("Buying Price:", priceEdit);
        layout.addRow("Exchange:", exchangeEdit);
        
        QPushButton* saveBtn = new QPushButton("Save");
        layout.addWidget(saveBtn);
        
        connect(saveBtn, &QPushButton::clicked, [&]() {
            QString symbol = symbolEdit->text();
            int shares = sharesEdit->text().toInt();
            double price = priceEdit->text().toDouble();
            QString exchange = exchangeEdit->text();
            
            if (!symbol.isEmpty() && shares > 0 && price > 0) {
                QSqlQuery query(db);
                query.prepare("INSERT INTO Holding (Symbol, Shares, CostPerShare, Date, uid) VALUES (?, ?, ?, ?, ?)");
                query.addBindValue(symbol);
                query.addBindValue(shares);
                query.addBindValue(price);
                query.addBindValue(QDateTime::currentSecsSinceEpoch());
                query.addBindValue(QVariant());
                
                if (query.exec()) {
                    loadHoldings();
                    loadData();
                    dialog.accept();
                }
            }
        });
        
        dialog.exec();
    }
    
    void closePosition() {
        int row = table->currentRow();
        if (row < 0) {
            QMessageBox::warning(this, "No Selection", "Please select a position to close");
            return;
        }
        
        QString symbol = table->item(row, 0)->text();
        // Remove " (Fund)" suffix if present
        symbol = symbol.replace(" (Fund)", "");
        
        // Get current holding info
        QSqlQuery query(db);
        query.prepare("SELECT Shares, CostPerShare FROM Holding WHERE Symbol = ?");
        query.addBindValue(symbol);
        
        if (!query.exec() || !query.next()) {
            QMessageBox::warning(this, "Error", "Could not find holding in database");
            return;
        }
        
        double currentShares = query.value(0).toDouble();
        double buyingPrice = query.value(1).toDouble();
        
        // Get current price
        double currentPrice = 0;
        QString priceText = table->item(row, 2)->text();
        currentPrice = priceText.toDouble();
        
        if (currentPrice <= 0) {
            QMessageBox::warning(this, "Error", "Invalid current price");
            return;
        }
        
        // Dialog to specify shares to close
        QDialog dialog(this);
        dialog.setWindowTitle("Close Position: " + symbol);
        
        QFormLayout layout(&dialog);
        
        QLabel* infoLabel = new QLabel(QString("Current Shares: %1\nBuying Price: $%2\nMarket Price: $%3")
                                        .arg(currentShares, 0, 'f', 2)
                                        .arg(buyingPrice, 0, 'f', 2)
                                        .arg(currentPrice, 0, 'f', 2));
        layout.addRow(infoLabel);
        
        QLineEdit* sharesEdit = new QLineEdit(QString::number(currentShares, 'f', 2));
        layout.addRow("Shares to Close:", sharesEdit);
        
        QLabel* priceLabel = new QLabel("Actual Selling Price:\n(Enter your real transaction price)");
        priceLabel->setStyleSheet("color: #FF9800; font-size: 10pt;");
        QLineEdit* priceEdit = new QLineEdit();
        priceEdit->setPlaceholderText("Enter actual selling price");
        layout.addRow(priceLabel, priceEdit);
        
        QPushButton* closeBtn = new QPushButton("Close Position");
        QPushButton* cancelBtn = new QPushButton("Cancel");
        QHBoxLayout* btnLayout = new QHBoxLayout();
        btnLayout->addWidget(closeBtn);
        btnLayout->addWidget(cancelBtn);
        layout.addRow(btnLayout);
        
        connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
        connect(closeBtn, &QPushButton::clicked, [&]() {
            double sharesToClose = sharesEdit->text().toDouble();
            double sellingPrice = priceEdit->text().toDouble();
            
            if (sharesToClose <= 0 || sharesToClose > currentShares) {
                QMessageBox::warning(&dialog, "Invalid Input", "Shares must be between 0 and " + QString::number(currentShares, 'f', 2));
                return;
            }
            
            if (sellingPrice <= 0) {
                QMessageBox::warning(&dialog, "Invalid Input", "Selling price must be greater than 0");
                return;
            }
            
            // Calculate realized P&L
            double realizedPL = (sellingPrice - buyingPrice) * sharesToClose;
            
            // Record realized P&L
            QSqlQuery insertQuery(db);
            insertQuery.prepare("INSERT INTO RealizedPL (symbol, shares, buying_price, selling_price, realized_pl, close_date) "
                              "VALUES (?, ?, ?, ?, ?, ?)");
            insertQuery.addBindValue(symbol);
            insertQuery.addBindValue(sharesToClose);
            insertQuery.addBindValue(buyingPrice);
            insertQuery.addBindValue(sellingPrice);
            insertQuery.addBindValue(realizedPL);
            insertQuery.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
            
            if (!insertQuery.exec()) {
                QMessageBox::critical(&dialog, "Error", "Failed to record realized P&L: " + insertQuery.lastError().text());
                return;
            }
            
            // Update or remove holding
            if (sharesToClose >= currentShares) {
                // Close entire position
                QSqlQuery deleteQuery(db);
                deleteQuery.prepare("DELETE FROM Holding WHERE Symbol = ?");
                deleteQuery.addBindValue(symbol);
                deleteQuery.exec();
            } else {
                // Partial close
                QSqlQuery updateQuery(db);
                updateQuery.prepare("UPDATE Holding SET Shares = ? WHERE Symbol = ?");
                updateQuery.addBindValue(currentShares - sharesToClose);
                updateQuery.addBindValue(symbol);
                updateQuery.exec();
            }
            
            // Refresh display
            loadHoldings();
            updateTable();
            updateCharts();
            
            QMessageBox::information(&dialog, "Success", 
                QString("Position closed:\n"
                        "Symbol: %1\n"
                        "Shares: %2\n"
                        "Buying Price: $%3\n"
                        "Selling Price: $%4\n"
                        "Realized P&L: $%5%6")
                .arg(symbol)
                .arg(sharesToClose, 0, 'f', 2)
                .arg(buyingPrice, 0, 'f', 2)
                .arg(sellingPrice, 0, 'f', 2)
                .arg(realizedPL >= 0 ? "+" : "")
                .arg(realizedPL, 0, 'f', 2));
            
            dialog.accept();
        });
        
        dialog.exec();
    }
    
    void zoomIn() {
        zoomLevel = qMin(2.0, zoomLevel + 0.1);
        applyZoom();
    }
    
    void zoomOut() {
        zoomLevel = qMax(0.5, zoomLevel - 0.1);
        applyZoom();
    }
    
    void zoomReset() {
        zoomLevel = 1.0;
        applyZoom();
    }
    
    void applyZoom() {
        int newFontSize = static_cast<int>(baseFontSize * zoomLevel);
        QFont font = table->font();
        font.setPointSize(newFontSize);
        table->setFont(font);
        table->verticalHeader()->setDefaultSectionSize(static_cast<int>(20 * zoomLevel));
    }
    
    double parseNumber(const QJsonValue& value) {
        if (value.isDouble()) {
            return value.toDouble();
        } else if (value.isString()) {
            QString str = value.toString();
            str.remove(',').remove('+');
            return str.toDouble();
        }
        return 0.0;
    }
    
    QList<QColor> generateColors(int n) {
        QList<QColor> colors;
        for (int i = 0; i < n; i++) {
            double hue = static_cast<double>(i) / n;
            double saturation = 0.7 + (i % 3) * 0.1;
            double value = 0.8 + (i % 2) * 0.15;
            colors.append(QColor::fromHsvF(hue, saturation, value));
        }
        return colors;
    }
    
    bool isMarketHours() {
        QDateTime currentUtc = QDateTime::currentDateTimeUtc();
        QTime currentTime = currentUtc.time();
        
        // Market hours: 08:30 - 13:30 GMT
        QTime marketOpen(8, 30, 0);
        QTime marketClose(13, 30, 0);
        
        return currentTime >= marketOpen && currentTime <= marketClose;
    }
    
    void autoRefresh() {
        // Check if still within market hours
        if (!isMarketHours()) {
            refreshTimer->stop();
            QMessageBox::information(this, "Market Closed", "Auto-refresh stopped: Market hours ended (08:30-13:30 GMT)");
            return;
        }
        
        // Refresh data
        loadData();
    }
    
    void showPeriodicInvestmentDialog() {
        QDialog dialog(this);
        dialog.setWindowTitle("Periodic Investment Plans");
        dialog.resize(700, 450);
        
        QVBoxLayout* layout = new QVBoxLayout(&dialog);
        
        // Table to show existing plans
        QTableWidget* table = new QTableWidget();
        table->setColumnCount(6);
        table->setHorizontalHeaderLabels({"ID", "Symbol", "Amount (NTD)", "Day of Month", "Next Date", "Enabled"});
        loadPeriodicInvestmentTable(table);
        layout->addWidget(table);
        
        // Add new plan section
        QHBoxLayout* addLayout = new QHBoxLayout();
        QLineEdit* symbolEdit = new QLineEdit();
        symbolEdit->setPlaceholderText("Symbol (Fund Code)");
        QLineEdit* amountEdit = new QLineEdit();
        amountEdit->setPlaceholderText("Amount (NTD)");
        QSpinBox* dayOfMonthSpin = new QSpinBox();
        dayOfMonthSpin->setRange(1, 28);
        dayOfMonthSpin->setValue(4);
        dayOfMonthSpin->setPrefix("Day ");
        
        QPushButton* addBtn = new QPushButton("Add Plan");
        connect(addBtn, &QPushButton::clicked, [this, symbolEdit, amountEdit, dayOfMonthSpin, table, &dialog]() {
            QString symbol = symbolEdit->text().trimmed();
            double amount = amountEdit->text().toDouble();
            int dayOfMonth = dayOfMonthSpin->value();
            
            if (symbol.isEmpty() || amount <= 0) {
                QMessageBox::warning(const_cast<QDialog*>(&dialog), "Error", "Please enter valid symbol and amount");
                return;
            }
            
            // Calculate next occurrence of this day
            QDate today = QDate::currentDate();
            QDate nextDate;
            if (today.day() < dayOfMonth) {
                // This month
                nextDate = QDate(today.year(), today.month(), dayOfMonth);
            } else {
                // Next month
                nextDate = today.addMonths(1);
                nextDate = QDate(nextDate.year(), nextDate.month(), dayOfMonth);
            }
            
            QSqlQuery query(db);
            query.prepare("INSERT INTO PeriodicInvestment (symbol, amount, interval_days, next_date, enabled) "
                         "VALUES (?, ?, ?, ?, 1)");
            query.addBindValue(symbol);
            query.addBindValue(amount);
            query.addBindValue(dayOfMonth); // Store day of month in interval_days
            query.addBindValue(QDateTime(nextDate, QTime(0, 0)).toSecsSinceEpoch());
            
            if (query.exec()) {
                loadPeriodicInvestmentTable(table);
                symbolEdit->clear();
                amountEdit->clear();
                QMessageBox::information(const_cast<QDialog*>(&dialog), "Success", "Periodic investment plan added");
            } else {
                QMessageBox::critical(const_cast<QDialog*>(&dialog), "Error", "Failed to add plan: " + query.lastError().text());
            }
        });
        
        QPushButton* deleteBtn = new QPushButton("Delete Selected");
        connect(deleteBtn, &QPushButton::clicked, [this, table, &dialog]() {
            int row = table->currentRow();
            if (row >= 0) {
                int id = table->item(row, 0)->text().toInt();
                QSqlQuery query(db);
                query.prepare("DELETE FROM PeriodicInvestment WHERE id = ?");
                query.addBindValue(id);
                if (query.exec()) {
                    loadPeriodicInvestmentTable(table);
                    QMessageBox::information(const_cast<QDialog*>(&dialog), "Success", "Plan deleted");
                }
            }
        });
        
        QPushButton* executeNowBtn = new QPushButton("Execute Now");
        connect(executeNowBtn, &QPushButton::clicked, [this]() {
            checkPeriodicInvestments();
        });
        
        addLayout->addWidget(new QLabel("Symbol:"));
        addLayout->addWidget(symbolEdit);
        addLayout->addWidget(new QLabel("Amount:"));
        addLayout->addWidget(amountEdit);
        addLayout->addWidget(new QLabel("Day of Month:"));
        addLayout->addWidget(dayOfMonthSpin);
        addLayout->addWidget(addBtn);
        addLayout->addWidget(deleteBtn);
        addLayout->addWidget(executeNowBtn);
        layout->addLayout(addLayout);
        
        dialog.exec();
    }
    
    void loadPeriodicInvestmentTable(QTableWidget* table) {
        table->setRowCount(0);
        QSqlQuery query("SELECT id, symbol, amount, interval_days, next_date, enabled FROM PeriodicInvestment", db);
        
        while (query.next()) {
            int row = table->rowCount();
            table->insertRow(row);
            table->setItem(row, 0, new QTableWidgetItem(query.value(0).toString()));
            table->setItem(row, 1, new QTableWidgetItem(query.value(1).toString()));
            table->setItem(row, 2, new QTableWidgetItem(QString::number(query.value(2).toDouble(), 'f', 2)));
            table->setItem(row, 3, new QTableWidgetItem(query.value(3).toString()));
            
            QDateTime nextDate = QDateTime::fromSecsSinceEpoch(query.value(4).toLongLong());
            table->setItem(row, 4, new QTableWidgetItem(nextDate.toString("yyyy-MM-dd")));
            table->setItem(row, 5, new QTableWidgetItem(query.value(5).toInt() ? "Yes" : "No"));
        }
        table->resizeColumnsToContents();
    }
    
    void checkPeriodicInvestments() {
        qDebug() << "Checking periodic investments...";
        QSqlQuery query(db);
        query.prepare("SELECT id, symbol, amount, interval_days FROM PeriodicInvestment WHERE enabled = 1 AND next_date <= ?");
        query.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
        
        if (!query.exec()) {
            qDebug() << "Failed to query periodic investments:" << query.lastError().text();
            return;
        }
        
        QList<QMap<QString, QVariant>> duePlans;
        while (query.next()) {
            QMap<QString, QVariant> plan;
            plan["id"] = query.value(0).toInt();
            plan["symbol"] = query.value(1).toString();
            plan["amount"] = query.value(2).toDouble();
            plan["interval_days"] = query.value(3).toInt();
            duePlans.append(plan);
        }
        
        qDebug() << "Found" << duePlans.size() << "due investments";
        
        for (const auto& plan : duePlans) {
            executePeriodicInvestment(plan);
        }
    }
    
    void executePeriodicInvestment(const QMap<QString, QVariant>& plan) {
        QString symbol = plan["symbol"].toString();
        double amount = plan["amount"].toDouble();
        int id = plan["id"].toInt();
        int dayOfMonth = plan["interval_days"].toInt(); // This stores day of month
        
        // Find current price from fund data
        double currentPrice = 0;
        for (const auto& fund : fundData) {
            if (fund["code"] == symbol) {
                currentPrice = fund["nav"].toDouble();
                break;
            }
        }
        
        if (currentPrice <= 0) {
            qDebug() << "Cannot execute investment for" << symbol << "- price not found or invalid";
            return;
        }
        
        // Calculate shares (can be fractional)
        double shares = amount / currentPrice;
        
        qDebug() << "Executing periodic investment:" << symbol << "Amount:" << amount << "Price:" << currentPrice << "Shares:" << shares;
        
        // Add to holdings (or update existing)
        QSqlQuery checkQuery(db);
        checkQuery.prepare("SELECT Shares, CostPerShare FROM Holding WHERE Symbol = ?");
        checkQuery.addBindValue(symbol);
        
        if (checkQuery.exec() && checkQuery.next()) {
            // Update existing holding (weighted average)
            double existingShares = checkQuery.value(0).toDouble();
            double existingCost = checkQuery.value(1).toDouble();
            double newTotalShares = existingShares + shares;
            double newAvgCost = ((existingShares * existingCost) + (shares * currentPrice)) / newTotalShares;
            
            QSqlQuery updateQuery(db);
            updateQuery.prepare("UPDATE Holding SET Shares = ?, CostPerShare = ?, Date = ? WHERE Symbol = ?");
            updateQuery.addBindValue(newTotalShares);
            updateQuery.addBindValue(newAvgCost);
            updateQuery.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
            updateQuery.addBindValue(symbol);
            
            if (!updateQuery.exec()) {
                qDebug() << "Failed to update holding:" << updateQuery.lastError().text();
                return;
            }
        } else {
            // Insert new holding
            QSqlQuery insertQuery(db);
            insertQuery.prepare("INSERT INTO Holding (Symbol, Shares, CostPerShare, Date) VALUES (?, ?, ?, ?)");
            insertQuery.addBindValue(symbol);
            insertQuery.addBindValue(shares);
            insertQuery.addBindValue(currentPrice);
            insertQuery.addBindValue(QDateTime::currentDateTime().toSecsSinceEpoch());
            
            if (!insertQuery.exec()) {
                qDebug() << "Failed to insert holding:" << insertQuery.lastError().text();
                return;
            }
        }
        
        // Calculate next occurrence (same day next month)
        QDate today = QDate::currentDate();
        QDate nextDate = today.addMonths(1);
        nextDate = QDate(nextDate.year(), nextDate.month(), dayOfMonth);
        
        // Update next investment date
        QSqlQuery updatePlanQuery(db);
        updatePlanQuery.prepare("UPDATE PeriodicInvestment SET next_date = ? WHERE id = ?");
        updatePlanQuery.addBindValue(QDateTime(nextDate, QTime(0, 0)).toSecsSinceEpoch());
        updatePlanQuery.addBindValue(id);
        updatePlanQuery.exec();
        
        // Refresh display
        loadHoldings();
        updateTable();
        updateCharts();
        
        QMessageBox::information(this, "Investment Executed", 
            QString("Periodic investment executed:\\nSymbol: %1\\nAmount: %2 NTD\\nPrice: %3\\nShares: %4")
            .arg(symbol).arg(amount, 0, 'f', 2).arg(currentPrice, 0, 'f', 4).arg(shares, 0, 'f', 4));
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    StockViewWindow window;
    window.show();
    
    return app.exec();
}

#include "stockview.moc"
