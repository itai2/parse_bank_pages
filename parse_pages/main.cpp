#include <QFile>
#include <QString>
#include <QStringRef>
#include <QByteArray>
#include <QDebug>
#include <QRegExp>
#include <QDate>
#include <QLocale>
#include <QTextStream>

static QString currentYear;
static double balance = 0;
static int shortestLine = 10000;
static QString shortestLineStr;
static double regularInSum = 0;
static double regularChequeInSum = 0;
static double regularOutSum = 0;
static QDate startingDate;
static QDate endingDate;

static QString extractField( const QString &line, int start, int length )
{
    if ( start + 1 > line.size() )
        return QString();
    int realLength = start + length < line.size() ? length : line.size() - start;
//    qDebug() << start << ", " << length;
    return QStringRef( &line, start, realLength ).toString();
}

static bool parseMonthLine( const QString &line )
{
    QRegExp monthLineRegExp( ".*(ינואר|פברואר|מרץ|אפריל|מאי|יוני|יולי|אוגוסט|ספטמבר|אוקטובר|נובמבר|דצמבר).*(\\d\\d\\d\\d).*" );
    if ( monthLineRegExp.exactMatch( line ) )
    {
        currentYear = monthLineRegExp.cap( 2 );
        return true;
    }
    return false;
}

static QDate convertDate( const QString &dateStr )
{
    return QDate::fromString( dateStr + "/" + currentYear, "dd/MM/yyyy" );
}

static bool parseReferenceLine( QTextStream &output, const QString &line )
{
    enum {
        DATE1_START = 1,
        DATE1_SIZE = 5,
        DATE2_START = 7,
        DATE2_SIZE = 5,
        DESCRIPTION_START = 13,
        DESCRIPTION_SIZE = 33,
        REFERENCE_START = 47,
        REFERENCE_SIZE = 9,
        CREDIT_START = 56,
        CREDIT_SIZE = 12,
        DEBIT_START = 70,
        DEBIT_SIZE = 11,
    };

    if ( line.size() < DATE1_START + DATE1_SIZE )
        return false;

    QStringRef date1Str( &line, DATE1_START, DATE1_SIZE );

    if ( !QRegExp( "\\d\\d/\\d\\d" ).exactMatch( date1Str.toString() ) )
        return false;

    if ( line.size() < shortestLine )
    {
        shortestLine = line.size();
        shortestLineStr = line;
    }

    QDate date1 = convertDate( date1Str.toString() );
    if ( date1 < startingDate || date1 > endingDate )
        return true;

    QStringRef date2Str( &line, DATE2_START, DATE2_SIZE );
    QDate date2 = convertDate( date2Str.toString() );

    QString description = extractField( line, DESCRIPTION_START, DESCRIPTION_SIZE );
    QString reference = extractField( line, REFERENCE_START, REFERENCE_SIZE );
    QString creditStr = extractField( line, CREDIT_START, CREDIT_SIZE );
    QString debitStr = extractField( line, DEBIT_START, DEBIT_SIZE );

    bool creditOk;
    double credit = QLocale(QLocale::Hebrew).toDouble( creditStr.trimmed().split( "\n" ).at(0), &creditOk );
    if ( !creditOk )
        credit = 0;

    bool debitOk;
    double debit = QLocale(QLocale::Hebrew).toDouble( debitStr.trimmed().split( "\n" ).at(0), &debitOk );
    if ( !debitOk )
        debit = 0;

    if ( !creditOk && !debitOk )
    {
        qDebug() << "Double convertion failed: " << creditStr.trimmed() << ", " << debitStr.trimmed();
    }

    balance += credit;
    balance -= debit;
    output
            << date1.toString( "yyyy-MM-dd" )
            << "| "
            << date2.toString( "yyyy-MM-dd" )
            << "| "
            << description.trimmed()
            << "| "
            << reference.trimmed()
            << "| "
            << credit //<< "(" << creditStr.trimmed().split( "\n" ).at(0) << ")"
            << "| "
            << -debit //<< "(" << debitStr.trimmed().split( "\n" ).at(0) << ")"
            << "| "
            << balance;

    //regular income
    output << "| ";
    if ( description.contains( "משכורת" ) ||
         description.contains( "ביטוח לאומי - ילדים" ) ||
         description.contains( "ח משרד ראש" ) ||
         description.contains( "אדגו" ) ||
         description.contains( "טלדור מערכ" ) ||
         description.contains( "זקיפת רבית זכות" ) )
    {
        output << credit;
        regularInSum += credit;
    }
    else
        output << 0;

    //cheque in
    output << "| ";
    if ( description.contains( "הפקדת שיק" ) )
    {
        output << credit;
        regularChequeInSum += credit;
    }
    else
        output << 0;

    //regular outcome
    output << "| ";
    if ( description.contains( "הפקדה ל" ) ||
         description.contains( "חידוש פיק" ) ||
         description.contains( "קניית ני" ) )
        output << 0;
    else
    {
        output << -debit;
        regularOutSum -= debit;
    }

    output << "\n";

    return true;
}

int main(int argc, char *argv[])
{
    if ( argc != 4 )
    {
        qDebug() << "Usage: " << argv[0] << " <pages file> <start date (e.g. 2018-1-23)> <end date>";
        return -1;
    }

    startingDate = QDate::fromString( argv[2], "yyyy-M-d" );
    if ( !startingDate.isValid() )
    {
        qDebug() << "Starting date is not valid";
    }

    endingDate = QDate::fromString( argv[3], "yyyy-M-d" );
    if ( !endingDate.isValid() )
    {
        qDebug() << "Ending date is not valid";
    }

    QTextStream out( stdout );
    out.setRealNumberPrecision( 25 );

    QFile pagesFile( argv[1] );
    pagesFile.open( QFile::ReadOnly );

    out << "date1|date1|description|reference|credit|debit|balance|regular income|cheque in|regular outcome" << "\n";
    while( !pagesFile.atEnd() )
    {
        QString line = pagesFile.readLine();
        if ( parseReferenceLine( out, line ) )
            continue;
        if ( parseMonthLine( line ) )
            continue;
    }
    out << "date1|date1|description|reference|credit|debit|balance|" << regularInSum << "|" << regularChequeInSum << "|" << regularOutSum << "|" << regularInSum + regularOutSum << "\n";
    out << "date1|date1|description|reference|credit|debit|balance|regular income|cheque in|regular outcome" << "\n";

//    qDebug() << "Shortest line: " << shortestLine;
//    qDebug() << "Shortest line str: " << shortestLineStr;

    return 0;
}
