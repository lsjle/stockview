import tkinter as tk
from tkinter import ttk, messagebox, simpledialog
import requests
import json
import os
import sqlite3
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
from matplotlib import font_manager
from datetime import datetime
import colorsys


class StockTableWindow:
    def __init__(self, root):
        self.root = root
        self.root.title("Portfolio Holdings Viewer")
        self.root.geometry("1400x900")
        
        # Define portfolio columns
        self.columns = [
            "Symbol", "Exchange", "Stock Name", "Current Price", "Price Change", 
            "% Change", "# Shares", "Current Value", "Buying Price", "Cost", 
            "Unrealized P&L", "% U P&L"
        ]
        
        # Holdings data
        self.holdings = self.load_holdings()
        self.fund_data = []
        
        # Table zoom level
        self.zoom_level = 1.0
        self.base_font_size = 9
        
        # Configure Chinese font for matplotlib
        self.setup_chinese_font()
        
        # Configure grid weight
        root.columnconfigure(0, weight=1)
        root.rowconfigure(0, weight=0)  # Chart area
        root.rowconfigure(1, weight=1)  # Table area
        
        # Create frame for charts
        self.chart_frame = ttk.Frame(root)
        self.chart_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S), padx=10, pady=5)
        
        # Create matplotlib figure for charts
        self.fig = Figure(figsize=(14, 4))
        self.canvas = FigureCanvasTkAgg(self.fig, master=self.chart_frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        
        # Create frame for the table
        frame = ttk.Frame(root, padding="10")
        frame.grid(row=1, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Configure grid weight for table frame
        frame.columnconfigure(0, weight=1)
        frame.rowconfigure(0, weight=1)
        
        # Create Treeview (table)
        self.tree = ttk.Treeview(frame, columns=self.columns, show='headings')
        
        # Configure column headings and widths
        column_widths = {
            "Symbol": 80, "Exchange": 80, "Stock Name": 150, "Current Price": 100,
            "Price Change": 100, "% Change": 80, "# Shares": 80, "Current Value": 120,
            "Buying Price": 100, "Cost": 120, "Unrealized P&L": 120, "% U P&L": 80
        }
        
        for col in self.columns:
            self.tree.heading(col, text=col, command=lambda c=col: self.sort_by(c, False))
            width = column_widths.get(col, 100)
            self.tree.column(col, width=width, anchor='center')
        
        # Add scrollbars
        vsb = ttk.Scrollbar(frame, orient="vertical", command=self.tree.yview)
        hsb = ttk.Scrollbar(frame, orient="horizontal", command=self.tree.xview)
        self.tree.configure(yscrollcommand=vsb.set, xscrollcommand=hsb.set)
        
        # Grid layout
        self.tree.grid(column=0, row=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        vsb.grid(column=1, row=0, sticky=(tk.N, tk.S))
        hsb.grid(column=0, row=1, sticky=(tk.W, tk.E))
        
        # Button frame
        btn_frame = ttk.Frame(frame)
        btn_frame.grid(column=0, row=2, pady=10, columnspan=2)
        
        # Add buttons
        refresh_btn = ttk.Button(btn_frame, text="Refresh Data", command=self.load_data)
        refresh_btn.pack(side=tk.LEFT, padx=5)
        
        update_btn = ttk.Button(btn_frame, text="Update & Record", command=self.update_and_record)
        update_btn.pack(side=tk.LEFT, padx=5)
        
        add_btn = ttk.Button(btn_frame, text="Add Holding", command=self.add_holding)
        add_btn.pack(side=tk.LEFT, padx=5)
        
        # Zoom controls
        ttk.Label(btn_frame, text="  Table Zoom:").pack(side=tk.LEFT, padx=5)
        zoom_in_btn = ttk.Button(btn_frame, text="+", width=3, command=self.zoom_in)
        zoom_in_btn.pack(side=tk.LEFT, padx=2)
        zoom_out_btn = ttk.Button(btn_frame, text="-", width=3, command=self.zoom_out)
        zoom_out_btn.pack(side=tk.LEFT, padx=2)
        zoom_reset_btn = ttk.Button(btn_frame, text="Reset", width=6, command=self.zoom_reset)
        zoom_reset_btn.pack(side=tk.LEFT, padx=2)
        
        # Bind keyboard shortcuts for zoom
        self.root.bind('<Control-plus>', lambda e: self.zoom_in())
        self.root.bind('<Control-equal>', lambda e: self.zoom_in())  # For keyboards where + is Shift+=
        self.root.bind('<Control-minus>', lambda e: self.zoom_out())
        self.root.bind('<Control-0>', lambda e: self.zoom_reset())  # Bonus: Ctrl+0 to reset
        
        # Load initial data
        self.load_fund_data()
        self.load_data()
        self.update_charts()
    
    def setup_chinese_font(self):
        """Setup Chinese font for matplotlib"""
        font_path = "NSTC.ttf"
        if os.path.exists(font_path):
            try:
                # Add the Chinese font
                font_manager.fontManager.addfont(font_path)
                font_prop = font_manager.FontProperties(fname=font_path)
                chinese_font_name = font_prop.get_name()
                
                # Use font fallback: Chinese font for CJK, sans-serif for Latin
                plt.rcParams['font.sans-serif'] = [chinese_font_name, 'DejaVu Sans', 'Arial', 'sans-serif']
                plt.rcParams['font.family'] = 'sans-serif'
                plt.rcParams['axes.unicode_minus'] = False  # Fix minus sign display
                self.chinese_font = font_prop
                print(f"Chinese font loaded: {chinese_font_name}")
            except Exception as e:
                print(f"Error loading Chinese font: {e}")
                self.chinese_font = None
                plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial', 'sans-serif']
        else:
            print(f"Font file {font_path} not found")
            self.chinese_font = None
            plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Arial', 'sans-serif']
    
    def zoom_in(self):
        """Increase table font size"""
        self.zoom_level = min(2.0, self.zoom_level + 0.1)
        self.apply_zoom()
    
    def zoom_out(self):
        """Decrease table font size"""
        self.zoom_level = max(0.5, self.zoom_level - 0.1)
        self.apply_zoom()
    
    def zoom_reset(self):
        """Reset table font size to default"""
        self.zoom_level = 1.0
        self.apply_zoom()
    
    def apply_zoom(self):
        """Apply zoom level to table"""
        new_font_size = int(self.base_font_size * self.zoom_level)
        style = ttk.Style()
        style.configure('Treeview', rowheight=int(20 * self.zoom_level), font=('TkDefaultFont', new_font_size))
        style.configure('Treeview.Heading', font=('TkDefaultFont', new_font_size, 'bold'))
    
    def load_holdings(self):
        """Load holdings from SQLite database"""
        db_file = "sqlite3.db"
        holdings = []
        
        if not os.path.exists(db_file):
            messagebox.showerror("Error", f"Database file '{db_file}' not found")
            return holdings
        
        try:
            conn = sqlite3.connect(db_file)
            cursor = conn.cursor()
            
            # Query the Holding table
            cursor.execute("SELECT Symbol, Shares, CostPerShare, Date, uid FROM Holding")
            rows = cursor.fetchall()
            
            for row in rows:
                symbol, shares, cost_per_share, date, uid = row
                holdings.append({
                    "symbol": str(symbol),
                    "shares": shares,
                    "buying_price": cost_per_share,
                    "exchange": "TWSE",
                    "date": date,
                    "uid": uid
                })
            
            conn.close()
        except Exception as e:
            messagebox.showerror("Error", f"Failed to load holdings from database: {e}")
        
        return holdings
    
    def save_historical_data(self, total_value, total_pl):
        """Save historical portfolio data to database"""
        db_file = "sqlite3.db"
        try:
            conn = sqlite3.connect(db_file)
            cursor = conn.cursor()
            
            # Create history table if it doesn't exist
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS PortfolioHistory (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp INTEGER NOT NULL,
                    total_value REAL NOT NULL,
                    total_pl REAL NOT NULL
                )
            """)
            
            # Insert current data
            timestamp = int(datetime.now().timestamp())
            cursor.execute("""
                INSERT INTO PortfolioHistory (timestamp, total_value, total_pl)
                VALUES (?, ?, ?)
            """, (timestamp, total_value, total_pl))
            
            conn.commit()
            conn.close()
        except Exception as e:
            print(f"Error saving historical data: {e}")
    
    def load_historical_data(self):
        """Load historical portfolio data from database"""
        db_file = "sqlite3.db"
        history = []
        
        try:
            conn = sqlite3.connect(db_file)
            cursor = conn.cursor()
            
            # Check if table exists
            cursor.execute("""
                SELECT name FROM sqlite_master 
                WHERE type='table' AND name='PortfolioHistory'
            """)
            
            if cursor.fetchone():
                cursor.execute("""
                    SELECT timestamp, total_value, total_pl 
                    FROM PortfolioHistory 
                    ORDER BY timestamp ASC
                """)
                history = cursor.fetchall()
            
            conn.close()
        except Exception as e:
            print(f"Error loading historical data: {e}")
        
        return history
    
    def update_and_record(self):
        """Update data and record to history"""
        market_data = self.fetch_stock_data()
        
        if not market_data:
            messagebox.showwarning("Warning", "Could not fetch market data")
            return
        
        stock_dict = {stock['Code']: stock for stock in market_data}
        total_value = 0
        total_pl = 0
        
        for holding in self.holdings:
            symbol = holding['symbol']
            shares = holding['shares']
            buying_price = holding['buying_price']
            stock_info = stock_dict.get(symbol)
            
            if stock_info:
                current_price = self.parse_number(stock_info.get('ClosingPrice', 0))
                if current_price > 0:
                    current_value = current_price * shares
                    cost = buying_price * shares
                    unrealized_pl = current_value - cost
                    total_value += current_value
                    total_pl += unrealized_pl
        
        # Save to history
        self.save_historical_data(total_value, total_pl)
        
        # Refresh display
        self.load_data()
        self.update_charts()
        
        messagebox.showinfo("Success", f"Portfolio data recorded!\nTotal Value: ${total_value:,.2f}\nTotal P&L: ${total_pl:+,.2f}")
    
    def save_holdings(self):
        """Save holdings to database"""
        db_file = "sqlite3.db"
        try:
            conn = sqlite3.connect(db_file)
            cursor = conn.cursor()
            
            # Get the last holding added (assuming it's the last in the list)
            if self.holdings:
                last_holding = self.holdings[-1]
                
                # Insert new holding
                cursor.execute("""
                    INSERT INTO Holding (Symbol, Shares, CostPerShare, Date, uid)
                    VALUES (?, ?, ?, ?, ?)
                """, (
                    last_holding['symbol'],
                    last_holding['shares'],
                    last_holding['buying_price'],
                    last_holding.get('date', 0),
                    last_holding.get('uid', None)
                ))
                
                conn.commit()
            conn.close()
        except Exception as e:
            messagebox.showerror("Error", f"Failed to save holding to database: {e}")
    
    def fetch_stock_data(self):
        """Fetch stock data from API"""
        headers = {
            'accept': 'application/json',
            'If-Modified-Since': 'Mon, 26 Jul 1997 05:00:00 GMT',
            'Cache-Control': 'no-cache',
            'Pragma': 'no-cache',
        }
        try:
            response = requests.get(
                'https://openapi.twse.com.tw/v1/exchangeReport/STOCK_DAY_ALL',
                headers=headers
            )
            response.raise_for_status()
            return response.json()
        except Exception as e:
            print(f"Error fetching data: {e}")
            return []
    
    def load_fund_data(self):
        """Load fund data from SITCA CSV"""
        try:
            url = "https://www.sitca.org.tw/MemberK0000/F/03/nav.csv"
            headers = {
                'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36',
                'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,text/csv,*/*;q=0.8'
            }
            response = requests.get(url, headers=headers, timeout=10)
            response.raise_for_status()
            
            # Parse CSV (Big5 encoding)
            content = response.content.decode('big5', errors='ignore')
            lines = content.strip().split('\n')
            
            self.fund_data = []
            # Skip header line
            for line in lines[1:]:
                fields = line.split(',')
                if len(fields) >= 7:
                    fund = {
                        'code': fields[3].strip(),  # 基金統編 (column 4)
                        'name': fields[5].strip(),  # 基金名稱 (column 6)
                        'nav': fields[6].strip()    # 基金淨值 (column 7)
                    }
                    self.fund_data.append(fund)
            
            print(f"Loaded {len(self.fund_data)} funds")
        except Exception as e:
            print(f"Error loading fund data: {e}")
            self.fund_data = []
    
    def load_data(self):
        """Load data into the table"""
        # Reload holdings from database
        self.holdings = self.load_holdings()
        
        # Reload fund data
        self.load_fund_data()
        
        # Clear existing data
        for item in self.tree.get_children():
            self.tree.delete(item)
        
        # Fetch market data
        market_data = self.fetch_stock_data()
        
        if not market_data:
            messagebox.showwarning("Warning", "Could not fetch market data")
            return
        
        # Create dictionaries for quick lookup
        stock_dict = {stock['Code']: stock for stock in market_data}
        fund_dict = {fund['code']: fund for fund in self.fund_data}
        
        # Process each holding
        for holding in self.holdings:
            symbol = holding['symbol']
            shares = holding['shares']
            buying_price = holding['buying_price']
            exchange = holding.get('exchange', 'TWSE')
            
            # Check if it's a stock
            stock_info = stock_dict.get(symbol)
            if stock_info:
                stock_name = stock_info.get('Name', 'N/A')
                current_price = self.parse_number(stock_info.get('ClosingPrice', 0))
                change = self.parse_number(stock_info.get('Change', 0))
                
                # Calculate metrics
                if current_price > 0:
                    pct_change = (change / (current_price - change)) * 100 if (current_price - change) != 0 else 0
                else:
                    pct_change = 0
                
                current_value = current_price * shares
                cost = buying_price * shares
                unrealized_pl = current_value - cost
                pct_upl = (unrealized_pl / cost * 100) if cost != 0 else 0
                
                # Format values
                values = [
                    symbol,
                    exchange,
                    stock_name,
                    f"{current_price:,.2f}",
                    f"{change:+.2f}",
                    f"{pct_change:+.2f}%",
                    f"{shares:,}",
                    f"{current_value:,.2f}",
                    f"{buying_price:,.2f}",
                    f"{cost:,.2f}",
                    f"{unrealized_pl:+,.2f}",
                    f"{pct_upl:+.2f}%"
                ]
                
                # Insert with color based on P&L
                item_id = self.tree.insert('', tk.END, values=values)
                if unrealized_pl > 0:
                    self.tree.item(item_id, tags=('profit',))
                elif unrealized_pl < 0:
                    self.tree.item(item_id, tags=('loss',))
            
            # Check if it's a fund
            elif fund_dict.get(symbol):
                fund_info = fund_dict[symbol]
                fund_name = fund_info.get('name', 'N/A')
                current_price = self.parse_number(fund_info.get('nav', 0))
                
                current_value = current_price * shares
                cost = buying_price * shares
                unrealized_pl = current_value - cost
                pct_upl = (unrealized_pl / cost * 100) if cost != 0 else 0
                change = current_price - buying_price
                pct_change = (change / buying_price * 100) if buying_price != 0 else 0
                
                # Format values
                values = [
                    symbol + " (Fund)",
                    "SITCA",
                    fund_name,
                    f"{current_price:,.2f}",
                    f"{change:+.2f}",
                    f"{pct_change:+.2f}%",
                    f"{shares:,}",
                    f"{current_value:,.2f}",
                    f"{buying_price:,.2f}",
                    f"{cost:,.2f}",
                    f"{unrealized_pl:+,.2f}",
                    f"{pct_upl:+.2f}%"
                ]
                
                # Insert with color based on P&L
                item_id = self.tree.insert('', tk.END, values=values)
                if unrealized_pl > 0:
                    self.tree.item(item_id, tags=('profit',))
                elif unrealized_pl < 0:
                    self.tree.item(item_id, tags=('loss',))
            
            else:
                # Not found in either stock or fund data
                values = [
                    symbol, exchange, "N/A", "N/A", "N/A", "N/A",
                    f"{shares:,}", "N/A", f"{buying_price:,.2f}", 
                    f"{buying_price * shares:,.2f}", "N/A", "N/A"
                ]
                self.tree.insert('', tk.END, values=values)
        
        # Configure tags for colors
        self.tree.tag_configure('profit', foreground='green')
        self.tree.tag_configure('loss', foreground='red')
    
    def parse_number(self, value):
        """Parse number from string, handling various formats"""
        if isinstance(value, (int, float)):
            return float(value)
        if isinstance(value, str):
            try:
                return float(value.replace(',', '').replace('+', ''))
            except:
                return 0.0
        return 0.0
    
    def add_holding(self):
        """Add a new holding"""
        # Create dialog window
        dialog = tk.Toplevel(self.root)
        dialog.title("Add Holding")
        dialog.geometry("300x200")
        
        ttk.Label(dialog, text="Symbol:").grid(row=0, column=0, padx=5, pady=5)
        symbol_entry = ttk.Entry(dialog)
        symbol_entry.grid(row=0, column=1, padx=5, pady=5)
        
        ttk.Label(dialog, text="Shares:").grid(row=1, column=0, padx=5, pady=5)
        shares_entry = ttk.Entry(dialog)
        shares_entry.grid(row=1, column=1, padx=5, pady=5)
        
        ttk.Label(dialog, text="Buying Price:").grid(row=2, column=0, padx=5, pady=5)
        price_entry = ttk.Entry(dialog)
        price_entry.grid(row=2, column=1, padx=5, pady=5)
        
        ttk.Label(dialog, text="Exchange:").grid(row=3, column=0, padx=5, pady=5)
        exchange_entry = ttk.Entry(dialog)
        exchange_entry.insert(0, "TWSE")
        exchange_entry.grid(row=3, column=1, padx=5, pady=5)
        
        def save_holding():
            try:
                symbol = symbol_entry.get().strip()
                shares = int(shares_entry.get())
                buying_price = float(price_entry.get())
                exchange = exchange_entry.get().strip() or "TWSE"
                
                if symbol and shares > 0 and buying_price > 0:
                    self.holdings.append({
                        "symbol": symbol,
                        "shares": shares,
                        "buying_price": buying_price,
                        "exchange": exchange
                    })
                    self.save_holdings()
                    self.load_data()
                    dialog.destroy()
                else:
                    messagebox.showerror("Error", "Please fill all fields with valid values")
            except ValueError:
                messagebox.showerror("Error", "Invalid number format")
        
        ttk.Button(dialog, text="Save", command=save_holding).grid(row=4, column=0, columnspan=2, pady=10)
    
    def generate_colors(self, n):
        """Generate n distinct colors using HSV color space"""
        colors = []
        for i in range(n):
            hue = i / n
            saturation = 0.7 + (i % 3) * 0.1  # Vary saturation
            value = 0.8 + (i % 2) * 0.15  # Vary brightness
            rgb = colorsys.hsv_to_rgb(hue, saturation, value)
            colors.append('#{:02x}{:02x}{:02x}'.format(int(rgb[0]*255), int(rgb[1]*255), int(rgb[2]*255)))
        return colors
    
    def update_charts(self):
        """Update the charts displayed above the table"""
        # Clear previous charts
        self.fig.clear()
        
        # Fetch market data for current prices
        market_data = self.fetch_stock_data()
        
        if not market_data:
            return
        
        # Create dictionaries for quick lookup
        stock_dict = {stock['Code']: stock for stock in market_data}
        fund_dict = {fund['code']: fund for fund in self.fund_data}
        
        # Prepare data for pie chart
        labels = []
        values = []
        pl_values = []
        
        total_value = 0
        
        for holding in self.holdings:
            symbol = holding['symbol']
            shares = holding['shares']
            buying_price = holding['buying_price']
            
            # Check if it's a stock
            stock_info = stock_dict.get(symbol)
            if stock_info:
                current_price = self.parse_number(stock_info.get('ClosingPrice', 0))
                
                if current_price > 0:
                    current_value = current_price * shares
                    cost = buying_price * shares
                    unrealized_pl = current_value - cost
                    
                    stock_name = stock_info.get('Name', 'N/A')
                    labels.append(f"{symbol}\n{stock_name[:8]}")
                    values.append(current_value)
                    pl_values.append(unrealized_pl)
                    total_value += current_value
            
            # Check if it's a fund
            elif fund_dict.get(symbol):
                fund_info = fund_dict[symbol]
                current_price = self.parse_number(fund_info.get('nav', 0))
                
                if current_price > 0:
                    current_value = current_price * shares
                    cost = buying_price * shares
                    unrealized_pl = current_value - cost
                    
                    fund_name = fund_info.get('name', 'N/A')
                    labels.append(f"{symbol} (Fund)\n{fund_name[:8]}")
                    values.append(current_value)
                    pl_values.append(unrealized_pl)
                    total_value += current_value
        
        # Generate diverse colors
        colors = self.generate_colors(len(values)) if values else []
        
        # First subplot - Portfolio distribution pie chart
        ax1 = self.fig.add_subplot(121)
        if values:
            # Create pie chart with colorful palette
            wedges, texts, autotexts = ax1.pie(
                values, 
                labels=labels, 
                colors=colors,
                autopct='%1.1f%%',
                startangle=90,
                textprops={'fontsize': 10}
            )
            
            # Set title with Chinese font support
            title_props = {'fontsize': 14, 'fontweight': 'bold', 'pad': 10}
            ax1.set_title(f'Portfolio Distribution - Total: ${total_value:,.2f}', **title_props)
            
            # Style the text
            for text, autotext, pl in zip(texts, autotexts, pl_values):
                autotext.set_color('white')
                autotext.set_fontweight('bold')
                autotext.set_fontsize(10)
                
        
        # Second subplot - Historical data line chart
        ax2 = self.fig.add_subplot(122)
        history = self.load_historical_data()
        
        if history and len(history) > 0:
            timestamps = [datetime.fromtimestamp(h[0]) for h in history]
            values_hist = [h[1] for h in history]
            pl_hist = [h[2] for h in history]
            
            # Plot total value
            ax2_twin = ax2.twinx()
            line1 = ax2.plot(timestamps, values_hist, 'b-o', label='Total Value', linewidth=2, markersize=4)
            line2 = ax2_twin.plot(timestamps, pl_hist, 'g-s', label='Total P&L', linewidth=2, markersize=4)
            
            label_props = {'fontsize': 9}
            
            ax2.set_xlabel('Date', **label_props)
            ax2.set_ylabel('Total Value ($)', color='b', **label_props)
            ax2_twin.set_ylabel('Total P&L ($)', color='g', **label_props)
            ax2.tick_params(axis='y', labelcolor='b', labelsize=8)
            ax2_twin.tick_params(axis='y', labelcolor='g', labelsize=8)
            ax2.tick_params(axis='x', rotation=45, labelsize=8)
            ax2.grid(True, alpha=0.3)
            
            # Combine legends
            lines = line1 + line2
            labels_legend = [l.get_label() for l in lines]
            legend_props = {'loc': 'upper left', 'fontsize': 8}
            ax2.legend(lines, labels_legend, **legend_props)
            
            title_props = {'fontsize': 11, 'fontweight': 'bold', 'pad': 10}
            ax2.set_title('Portfolio History', **title_props)
        else:
            text_props = {'ha': 'center', 'va': 'center', 'fontsize': 10, 'transform': ax2.transAxes}
            ax2.text(0.5, 0.5, 'No historical data yet\nClick "Update & Record" to start tracking', **text_props)
            
            title_props = {'fontsize': 11, 'fontweight': 'bold', 'pad': 10}
            ax2.set_title('Portfolio History', **title_props)
            ax2.axis('off')
        
        self.fig.tight_layout()
        self.canvas.draw()
    
    def sort_by(self, col, reverse):
        """Sort table by column"""
        data = [(self.tree.set(child, col), child) for child in self.tree.get_children('')]
        
        # Try to sort numerically if possible
        try:
            data.sort(key=lambda t: float(t[0].replace(',', '').replace('%', '').replace('+', '')), reverse=reverse)
        except:
            data.sort(reverse=reverse)
        
        for index, (val, child) in enumerate(data):
            self.tree.move(child, '', index)
        
        # Reverse sort next time
        self.tree.heading(col, command=lambda: self.sort_by(col, not reverse))


def main():
    root = tk.Tk()
    app = StockTableWindow(root)
    root.mainloop()


if __name__ == "__main__":
    main()
