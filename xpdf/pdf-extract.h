// pdf-extract.h
// @author octopoulo <polluxyz@gmail.com>
// @version 2024-08-09

struct PdfBitmap
{
	uint8_t* alpha    = nullptr; // alpha channel
	uint8_t* data     = nullptr; // rgb data
	int      height   = 0;       //
	int      mode     = 0;       // color mode
	int64_t  rowSize  = 0;       // row size (bytes)
	size_t   rowSizeA = 0;       // alpha row size (bytes)
	int      width    = 0;       //

	~PdfBitmap()
	{
		if (alpha) delete[] alpha;
		if (data) delete[] data;
	}
};

class PdfExtract
{
public:
	void* pdfDoc = nullptr;

	PdfExtract(const std::filesystem::path& configFile = "", int quiet = 0);
	~PdfExtract();

	void ClosePdf();
	void Destroy();

	/// Extract rendered bitmap of a page
	PdfBitmap ExtractBitmap(int page, double dpi = 150.0, int rotate = 0, bool alpha = false);

	/// Extract text of a page (no OCR)
	/// @param mode: 0: reading order, 1: physical, 2: simple 1 column, 3: simple 1 column2, 4: table, 5: line printer, 6: raw order
	std::string ExtractText(int page, int pageEnd, int mode = 4, double fixedLineSp = 0.0, double fixedPitch = 0.0);

	int  GetNumPages();
	void OpenPdf(const std::filesystem::path& filename);
};
