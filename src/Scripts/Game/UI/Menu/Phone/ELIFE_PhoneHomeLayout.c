//------------------------------------------------------------------------------------------------
//! One thing on the home grid: an app tile (one cell) or a widget (one row high, half or full width).
class ELIFE_PhoneHomeItem
{
	string m_sId;

	//! The app it opens, if m_bOpensApp.
	EPhoneScreenState m_eState;
	bool m_bOpensApp = true;

	//! A widget rather than an app tile.
	bool m_bCard;

	//! The widths a widget offers, its default first. Tiles have one.
	ref array<int> m_aWidths = {1};

	//! A widget that starts in the Widgets sheet rather than on the grid.
	bool m_bStartsRemoved;

	//! A widget the player took off the home screen. Apps are uninstalled instead.
	bool m_bRemoved;

	//! Top-left cell; -1 while unplaced, removed, or when the grid has no room for it.
	int m_iColumn = -1;
	int m_iRow = -1;
	int m_iWidth = 1;
	int m_iHeight = 1;

	//------------------------------------------------------------------------------------------------
	bool IsPlaced()
	{
		return m_iColumn >= 0 && m_iRow >= 0;
	}

	//------------------------------------------------------------------------------------------------
	bool CanResize()
	{
		return m_aWidths.Count() > 1;
	}

	//------------------------------------------------------------------------------------------------
	bool Overlaps(int column, int row, int width, int height)
	{
		return m_iColumn < column + width && column < m_iColumn + m_iWidth && m_iRow < row + height && row < m_iRow + m_iHeight;
	}

	//------------------------------------------------------------------------------------------------
	bool IsInside(int column, int row, int width, int height)
	{
		return m_iColumn >= column && m_iRow >= row && m_iColumn + m_iWidth <= column + width && m_iRow + m_iHeight <= row + height;
	}
}

//------------------------------------------------------------------------------------------------
//! Where everything sits on the home screen's fixed grid. Positions are free: gaps are allowed, and a
//! drop onto occupied cells swaps what is there into the cells the moved item left.
//! Saved as comma-separated entries: "id:column:row" for tiles, "id:column:row:width" for widgets.
//! Ids are app keys for tiles, CARD_PREFIX + app key for app cards, or a WIDGET id. A removed widget
//! is saved at column and row -1.
class ELIFE_PhoneHomeLayout
{
	static const int COLUMNS = 4;
	static const int ROWS = 7;

	//! Stored with the layout on the Bridge. Bump it when the saved format changes, and read older ones.
	static const int FORMAT_VERSION = 1;

	static const int FULL = COLUMNS;
	static const int HALF = COLUMNS / 2;

	static const string CARD_PREFIX = "Card.";
	static const string CLOCK_ID = "Widget.Clock";
	static const string WEATHER_ID = "Widget.Weather";
	static const string LOCATION_ID = "Widget.Location";
	static const string FAVOURITES_ID = "Widget.Favourites";

	protected ref array<ref ELIFE_PhoneHomeItem> m_aItems = {};

	//------------------------------------------------------------------------------------------------
	ELIFE_PhoneHomeItem AddTile(notnull ELIFE_PhoneAppEntry app)
	{
		ELIFE_PhoneHomeItem item = new ELIFE_PhoneHomeItem();
		item.m_sId = app.m_sAppKey;
		item.m_eState = app.m_eState;
		m_aItems.Insert(item);
		return item;
	}

	//------------------------------------------------------------------------------------------------
	//! widths lists what the widget can be, its default first.
	ELIFE_PhoneHomeItem AddWidget(string id, EPhoneScreenState state, bool opensApp, notnull array<int> widths, bool startsRemoved = false)
	{
		ELIFE_PhoneHomeItem item = new ELIFE_PhoneHomeItem();
		item.m_sId = id;
		item.m_eState = state;
		item.m_bOpensApp = opensApp;
		item.m_bCard = true;
		item.m_aWidths.Copy(widths);
		item.m_iWidth = widths[0];
		item.m_bStartsRemoved = startsRemoved;
		m_aItems.Insert(item);
		return item;
	}

	//------------------------------------------------------------------------------------------------
	array<ref ELIFE_PhoneHomeItem> GetItems()
	{
		return m_aItems;
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_PhoneHomeItem Find(string id)
	{
		foreach (ELIFE_PhoneHomeItem item : m_aItems)
		{
			if (ELIFE_PhoneAppCatalog.SameKey(item.m_sId, id))
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts every item at its saved cell where that is still free, then places the rest: widgets into
	//! the first free spot from the top in the order added, apps from the bottom up - as one block on a
	//! fresh layout, so the default reads like a dock under the widgets.
	void Place(string saved)
	{
		array<string> savedIds = {};

		foreach (ELIFE_PhoneHomeItem item : m_aItems)
		{
			item.m_iColumn = -1;
			item.m_iRow = -1;
			item.m_bRemoved = false;
			item.m_iWidth = item.m_aWidths[0];
		}

		array<string> entries = {};
		saved.Split(",", entries, true);

		array<string> parts = {};
		foreach (string entry : entries)
		{
			parts.Clear();
			entry.Split(":", parts, false);
			if (parts.Count() < 3)
				continue;

			ELIFE_PhoneHomeItem savedItem = Find(parts[0]);
			if (!savedItem || savedIds.Find(savedItem.m_sId) != -1)
				continue;

			savedIds.Insert(savedItem.m_sId);

			if (parts.Count() > 3 && savedItem.m_aWidths.Find(parts[3].ToInt()) != -1)
				savedItem.m_iWidth = parts[3].ToInt();

			int column = parts[1].ToInt();
			int row = parts[2].ToInt();
			if (column < 0 && savedItem.m_bCard)
			{
				savedItem.m_bRemoved = true;
				continue;
			}

			if (Fits(savedItem, column, row) && IsFree(column, row, savedItem.m_iWidth, savedItem.m_iHeight))
			{
				savedItem.m_iColumn = column;
				savedItem.m_iRow = row;
			}
		}

		foreach (ELIFE_PhoneHomeItem card : m_aItems)
		{
			if (!card.m_bCard || card.IsPlaced() || card.m_bRemoved)
				continue;

			//! Never saved: an opt-in widget waits in the sheet.
			if (card.m_bStartsRemoved && savedIds.Find(card.m_sId) == -1)
			{
				card.m_bRemoved = true;
				continue;
			}

			PlaceFromTop(card);
		}

		PlaceTiles();
	}

	//------------------------------------------------------------------------------------------------
	protected void PlaceTiles()
	{
		array<ELIFE_PhoneHomeItem> tiles = {};
		foreach (ELIFE_PhoneHomeItem item : m_aItems)
		{
			if (!item.m_bCard && !item.IsPlaced())
				tiles.Insert(item);
		}

		if (tiles.IsEmpty())
			return;

		//! A block of whole rows at the bottom, filled in catalog order.
		int rows = (tiles.Count() + COLUMNS - 1) / COLUMNS;
		int firstRow = ROWS - rows;
		if (firstRow >= 0 && IsFree(0, firstRow, COLUMNS, rows))
		{
			foreach (int i, ELIFE_PhoneHomeItem tile : tiles)
			{
				tile.m_iColumn = i % COLUMNS;
				tile.m_iRow = firstRow + i / COLUMNS;
			}

			return;
		}

		foreach (ELIFE_PhoneHomeItem single : tiles)
		{
			PlaceFromBottom(single);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool PlaceFromTop(notnull ELIFE_PhoneHomeItem item)
	{
		for (int row = 0; row < ROWS; row++)
		{
			for (int column = 0; column < COLUMNS; column++)
			{
				if (TryPlace(item, column, row))
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool PlaceFromBottom(notnull ELIFE_PhoneHomeItem item)
	{
		for (int row = ROWS - 1; row >= 0; row--)
		{
			for (int column = 0; column < COLUMNS; column++)
			{
				if (TryPlace(item, column, row))
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TryPlace(notnull ELIFE_PhoneHomeItem item, int column, int row)
	{
		if (!Fits(item, column, row) || !IsFree(column, row, item.m_iWidth, item.m_iHeight))
			return false;

		item.m_iColumn = column;
		item.m_iRow = row;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a widget off the grid.
	void Remove(notnull ELIFE_PhoneHomeItem item)
	{
		if (!item.m_bCard)
			return;

		item.m_bRemoved = true;
		item.m_iColumn = -1;
		item.m_iRow = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a removed widget back in the first free row. False if there is none.
	bool Restore(notnull ELIFE_PhoneHomeItem item)
	{
		if (!item.m_bRemoved || !PlaceFromTop(item))
			return false;

		item.m_bRemoved = false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Switches a widget to its next width, staying in its row: in place if it fits, else at the
	//! nearest column that does. False if the row has no room.
	bool Resize(notnull ELIFE_PhoneHomeItem item)
	{
		if (!item.CanResize() || !item.IsPlaced())
			return false;

		int index = item.m_aWidths.Find(item.m_iWidth);
		int width = item.m_aWidths[(index + 1) % item.m_aWidths.Count()];

		int bestColumn = -1;
		int bestDistance = COLUMNS;
		for (int column = 0; column + width <= COLUMNS; column++)
		{
			int distance = Math.AbsInt(column - item.m_iColumn);
			if (distance < bestDistance && IsFree(column, item.m_iRow, width, item.m_iHeight, item))
			{
				bestColumn = column;
				bestDistance = distance;
			}
		}

		if (bestColumn == -1)
			return false;

		item.m_iColumn = bestColumn;
		item.m_iWidth = width;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool HasRoomFor(notnull ELIFE_PhoneHomeItem item)
	{
		for (int row = 0; row + item.m_iHeight <= ROWS; row++)
		{
			for (int column = 0; column + item.m_iWidth <= COLUMNS; column++)
			{
				if (IsFree(column, row, item.m_iWidth, item.m_iHeight))
					return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Keeps previous's spots for items not on the grid now (a card still waiting on its data, an
	//! uninstalled app), so they come back where they were. Items here come first, so they win.
	string Serialize(string previous = "")
	{
		string saved = "";
		foreach (ELIFE_PhoneHomeItem item : m_aItems)
		{
			if (!item.IsPlaced() && !item.m_bRemoved)
				continue;

			if (saved != "")
				saved += ",";

			saved += item.m_sId + ":" + item.m_iColumn.ToString() + ":" + item.m_iRow.ToString();
			if (item.m_bCard)
				saved += ":" + item.m_iWidth.ToString();
		}

		array<string> entries = {};
		previous.Split(",", entries, true);

		array<string> parts = {};
		foreach (string entry : entries)
		{
			parts.Clear();
			entry.Split(":", parts, false);
			if (parts.Count() < 3 || Find(parts[0]))
				continue;

			if (saved != "")
				saved += ",";

			saved += entry;
		}

		return saved;
	}

	//------------------------------------------------------------------------------------------------
	//! The top-left cell an item would land on, clamped into the grid.
	void ClampToGrid(notnull ELIFE_PhoneHomeItem item, inout int column, inout int row)
	{
		column = Math.ClampInt(column, 0, COLUMNS - item.m_iWidth);
		row = Math.ClampInt(row, 0, ROWS - item.m_iHeight);
	}

	//------------------------------------------------------------------------------------------------
	//! True if item can go to (column, row): onto free cells, or onto items lying wholly inside its new
	//! footprint, which then swap into the footprint it leaves - those are put in displaced. Its own
	//! cell counts as no move.
	bool GetSwap(notnull ELIFE_PhoneHomeItem item, int column, int row, notnull array<ELIFE_PhoneHomeItem> displaced)
	{
		displaced.Clear();

		if (!item.IsPlaced() || !Fits(item, column, row))
			return false;

		if (column == item.m_iColumn && row == item.m_iRow)
			return false;

		foreach (ELIFE_PhoneHomeItem other : m_aItems)
		{
			if (other == item || !other.IsPlaced() || !other.Overlaps(column, row, item.m_iWidth, item.m_iHeight))
				continue;

			//! Half on, half off can't swap cleanly.
			if (!other.IsInside(column, row, item.m_iWidth, item.m_iHeight))
			{
				displaced.Clear();
				return false;
			}

			displaced.Insert(other);
		}

		//! Overlapping footprints would swap items onto themselves.
		if (!displaced.IsEmpty() && item.Overlaps(column, row, item.m_iWidth, item.m_iHeight))
		{
			displaced.Clear();
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool Move(notnull ELIFE_PhoneHomeItem item, int column, int row)
	{
		array<ELIFE_PhoneHomeItem> displaced = {};
		if (!GetSwap(item, column, row, displaced))
			return false;

		int shiftColumn = item.m_iColumn - column;
		int shiftRow = item.m_iRow - row;
		foreach (ELIFE_PhoneHomeItem other : displaced)
		{
			other.m_iColumn += shiftColumn;
			other.m_iRow += shiftRow;
		}

		item.m_iColumn = column;
		item.m_iRow = row;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Fits(notnull ELIFE_PhoneHomeItem item, int column, int row)
	{
		return column >= 0 && row >= 0 && column + item.m_iWidth <= COLUMNS && row + item.m_iHeight <= ROWS;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsFree(int column, int row, int width, int height, ELIFE_PhoneHomeItem ignore = null)
	{
		foreach (ELIFE_PhoneHomeItem item : m_aItems)
		{
			if (item != ignore && item.IsPlaced() && item.Overlaps(column, row, width, height))
				return false;
		}

		return true;
	}
}
