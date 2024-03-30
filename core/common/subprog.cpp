#include "subprog.h"
#include <stdexcept>

// プログラムの出力を読み出すストリーム。
std::shared_ptr<Stream> Subprogram::inputStream()
{
    if (!m_receiveData)
        throw std::runtime_error("no input stream");
    return m_inputStream;
}

std::shared_ptr<Stream> Subprogram::outputStream()
{
    if (!m_feedData)
        throw std::runtime_error("no output stream");
    return m_outputStream;
}

// プロセスID。
int Subprogram::pid()
{
    return m_pid;
}
