#include "bmp_image.h"
#include "pack_defines.h"

#include <array>
#include <fstream>
#include <string_view>
#include <string>

using namespace std;

namespace img_lib
{

    struct BitmapFileHeader
    {
        uint8_t type[2] = {'B', 'M'}; // Подпись — 2 байта. Символы BM.
        uint32_t size;                // Суммарный размер заголовка и данных — 4 байта, беззнаковое целое. Размер данных определяется как отступ, умноженный на высоту изображения.
        uint32_t reserved = 0;        // Зарезервированное пространство — 4 байта, заполненные нулями.
        uint32_t offset = 14 + 40;    // Отступ данных от начала файла — 4 байта, беззнаковое целое.Он равен размеру двух частей заголовка.
    };

    struct BitmapInfoHeader
    {
        uint32_t size = 40;                 // Размер заголовка — 4 байта, беззнаковое целое. Учитывается только размер второй части заголовка.
        int32_t width;                      // Ширина изображения в пикселях — 4 байта, знаковое целое.
        int32_t height;                     // Высота изображения в пикселях — 4 байта, знаковое целое.
        uint16_t plane_count = 1;           // Количество плоскостей — 2 байта, беззнаковое целое. В нашем случае всегда 1 — одна RGB плоскость.
        uint16_t bit_per_pix = 24;          // Количество бит на пиксель — 2 байта, беззнаковое целое. В нашем случае всегда 24.
        uint32_t compression = 0;           // Тип сжатия — 4 байта, беззнаковое целое. В нашем случае всегда 0 — отсутствие сжатия.
        uint32_t byte_count;                // Количество байт в данных — 4 байта, беззнаковое целое. Произведение отступа на высоту.
        int32_t h_resolution = 11811;       // Горизонтальное разрешение, пикселей на метр — 4 байта, знаковое целое. Нужно записать 11811, что примерно соответствует 300 DPI.
        int32_t v_resolution = 11811;       // Вертикальное разрешение, пикселей на метр — 4 байта, знаковое целое. Нужно записать 11811, что примерно соответствует 300 DPI.
        int32_t color_cnt = 0;              // Количество использованных цветов — 4 байта, знаковое целое. Нужно записать 0 — значение не определено.
        int32_t mean_color_cnt = 0x1000000; // Количество значимых цветов — 4 байта, знаковое целое. Нужно записать 0x1000000.
    };

    template <typename T>
    void Serialize(T val, std::ostream &out)
    {
        out.write(reinterpret_cast<const char *>(&val), sizeof(val));
    }

    template <typename T>
    void Deserialize(std::istream &in, T &val)
    {
        in.read(reinterpret_cast<char *>(&val), sizeof(val));
    }

    // функция вычисления отступа по ширине
    static int GetBMPStride(int w)
    {
        return 4 * ((w * 3 + 3) / 4);
    }

    // напишите эту функцию
    bool SaveBMP(const Path &file, const Image &image)
    {
        ofstream out(file, std::ios::binary);
        uint32_t padding = GetBMPStride(image.GetWidth());
        BitmapFileHeader file_header;
        BitmapInfoHeader info_header;
        file_header.size = (14 + 40 + padding * image.GetHeight());
        info_header.width = image.GetWidth();
        info_header.height = image.GetHeight();
        info_header.byte_count = image.GetHeight() * padding;

        Serialize(file_header.type[0], out);
        Serialize(file_header.type[1], out);
        Serialize(file_header.size, out);
        Serialize(file_header.reserved, out);
        Serialize(file_header.offset, out);

        Serialize(info_header.size, out);
        Serialize(info_header.width, out);
        Serialize(info_header.height, out);
        Serialize(info_header.plane_count, out);
        Serialize(info_header.bit_per_pix, out);
        Serialize(info_header.compression, out);
        Serialize(info_header.byte_count, out);
        Serialize(info_header.h_resolution, out);
        Serialize(info_header.v_resolution, out);
        Serialize(info_header.color_cnt, out);
        Serialize(info_header.mean_color_cnt, out);

        vector<char> buff(padding, 0);

        for (int y = info_header.height - 1; y >= 0; --y)
        {
            const auto line = image.GetLine(y);
            for (int x = 0; x < info_header.width; ++x)
            {
                buff[x * 3 + 0] = static_cast<char>(line[x].b);
                buff[x * 3 + 1] = static_cast<char>(line[x].g);
                buff[x * 3 + 2] = static_cast<char>(line[x].r);
            }
            out.write(buff.data(), buff.size());
        }
        return out.good();
    }

    // напишите эту функцию
    Image LoadBMP(const Path &file)
    {
        ifstream in(file, std::ios::binary);
        char trash[28];
        in.read(trash, 18);
        int32_t w, h;
        Deserialize(in, w);
        Deserialize(in, h);
        in.read(trash, 28);
        Image result(w, h, Color::Black());
        std::vector<char> buff(w * 3);

        for (int y = h - 1; y >= 0; --y)
        {
            Color *line = result.GetLine(y);
            in.read(buff.data(), w * 3);
            if (w % 2 != 0)
            {
                in.get();
            }
            for (int x = 0; x < w; ++x)
            {
                line[x].r = static_cast<byte>(buff[x * 3 + 2]);
                line[x].g = static_cast<byte>(buff[x * 3 + 1]);
                line[x].b = static_cast<byte>(buff[x * 3 + 0]);
            }
        }
        return result;
    }

} // namespace img_lib

// Данные в BMP записываются примерно как в PPM, но со следующими изменениями:
// строки идут в другом порядке, начиная с нижней,
// порядок компонент в пикселе обратный: Blue, Green, Red,
// отступ не всегда равен утроенной ширине: к каждой строке может добавляться padding, чтобы количество байт стало кратно четырём. Padding нужно заполнить нулевыми байтами. Формула для вычисления отступа приведена ниже.
// Главное — правильно записать и прочитать заголовок. А заголовок в BMP сложный. Он состоит из двух частей: Bitmap File Header и Bitmap Info Header.

// Bitmap File Header пишется в начале файла и имеет поля:

// Подпись — 2 байта. Символы BM.
// Суммарный размер заголовка и данных — 4 байта, беззнаковое целое. Размер данных определяется как отступ, умноженный на высоту изображения.
// Зарезервированное пространство — 4 байта, заполненные нулями.
// Отступ данных от начала файла — 4 байта, беззнаковое целое. Он равен размеру двух частей заголовка.

// Bitmap Info Header пишется после первой части и имеет поля:

// Размер заголовка — 4 байта, беззнаковое целое. Учитывается только размер второй части заголовка.
// Ширина изображения в пикселях — 4 байта, знаковое целое.
// Высота изображения в пикселях — 4 байта, знаковое целое.
// Количество плоскостей — 2 байта, беззнаковое целое. В нашем случае всегда 1 — одна RGB плоскость.
// Количество бит на пиксель — 2 байта, беззнаковое целое. В нашем случае всегда 24.
// Тип сжатия — 4 байта, беззнаковое целое. В нашем случае всегда 0 — отсутствие сжатия.
// Количество байт в данных — 4 байта, беззнаковое целое. Произведение отступа на высоту.
// Горизонтальное разрешение, пикселей на метр — 4 байта, знаковое целое. Нужно записать 11811, что примерно соответствует 300 DPI.
// Вертикальное разрешение, пикселей на метр — 4 байта, знаковое целое. Нужно записать 11811, что примерно соответствует 300 DPI.
// Количество использованных цветов — 4 байта, знаковое целое. Нужно записать 0 — значение не определено.
// Количество значимых цветов — 4 байта, знаковое целое. Нужно записать 0x1000000.
// Важно, что между полями отсутствует padding. Таким образом, размер первой части — 14 байт, размер второй части — 40 байт.
// Отдельную сложность представляет вычисление отступа, который не пишется в заголовке, но тем не менее не всегда равен утроенной ширине. В BMP отступ равен ширине, умноженной на три и округлённой вверх до числа, делящегося на четыре. Его можно вычислить по ширине такой функцией: