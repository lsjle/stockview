# Stock Portfolio Viewer - C++ Version

This is a C++ implementation of the stock portfolio viewer using Qt6.

## Features

- ✅ SQLite database integration for holdings
- ✅ Real-time stock data from Taiwan Stock Exchange API
- ✅ Interactive table with color-coded P&L
- ✅ Pie chart showing portfolio distribution
- ✅ Line chart showing historical performance
- ✅ Keyboard shortcuts (Ctrl+/-, Ctrl+0)
- ✅ Table zoom functionality
- ✅ Chinese font support (qiji.ttf)
- ✅ Colorful chart palette

## Requirements

- Qt6 (Core, Widgets, Network, Sql, Charts)
- CMake 3.16 or higher
- C++17 compiler (g++, clang, or MSVC)

## Installation

### Ubuntu/Debian
```bash
sudo apt install qt6-base-dev qt6-charts-dev cmake build-essential
```

### Arch Linux
```bash
sudo pacman -S qt6-base qt6-charts cmake gcc
```

### macOS (with Homebrew)
```bash
brew install qt@6 cmake
```

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

```bash
./stockview
```

Make sure `sqlite3.db` and `qiji.ttf` are in the same directory as the executable.

## Usage

- **Refresh Data**: Fetch latest stock prices
- **Update & Record**: Save current portfolio snapshot to history
- **Add Holding**: Add a new stock holding to database
- **Zoom Controls**: Use +/- buttons or Ctrl+/- keyboard shortcuts
- **Ctrl+0**: Reset zoom to default

## Database Schema

The application uses the same SQLite database as the Python version:
- `Holding` table: Stores stock holdings
- `PortfolioHistory` table: Auto-created for historical tracking

## Notes

- The application uses the same database file (`sqlite3.db`) as the Python version
- Chinese font file (`qiji.ttf`) should be in the working directory
- All features from the Python version are replicated
