#include "i18n.h"

#include <initializer_list>

namespace la {
namespace {

const Strings kZhCN = {
	L"zh-CN", false,
	L"使用说明", L"开机自启", L"退出", L"LatexAnywhere：Alt+L 打公式", L"Alt+L 已被其他程序占用，公式框暂时无法唤起。",
	L"LatexAnywhere 使用说明",
	L"LatexAnywhere 使用说明",
	L"在任何软件的输入框里，用 Obsidian + Latex Suite 的方式打公式。",
	L"基本用法",
	{L"在任意输入框里（微信、Word、浏览器、记事本……）按 ‹Alt›+‹L›，光标旁弹出公式框。",
	 L"直接打公式。框里就是原版 Latex Suite：比如 ‹x^2› 自动变成 ‹x^{2}›，‹//› 出分式，‹Tab› 跳到下一个空位。",
	 L"框上方实时显示渲染好的公式。",
	 L"按下面任意一种方式上屏，公式插回原来的输入框。"},
	L"先选中一段文字再按 ‹Alt›+‹L›，选中的内容会带进框里，改完上屏会替换原来的选区。",
	L"上屏方式", L"按键", L"插入的内容", L"适合",
	{{L"‹Enter›", L"公式本身 ‹x^{2}›", L"LaTeX 源码、Overleaf"},
	 {L"‹Alt›+‹Enter›", L"带 $ 的行内公式 ‹$x^{2}$›", L"Typora、Obsidian、Markdown"},
	 {L"‹Ctrl›+‹Enter›", L"Unicode 字符 ‹x²›", L"微信等不显示公式的地方"},
	 {L"‹Ctrl›+‹Shift›+‹Enter›", L"Word 原生公式（可双击编辑）", L"Word、PowerPoint"},
	 {L"‹Shift›+‹Enter›", L"在框里换行", L"多行公式"},
	 {L"‹Esc› 或再按 ‹Alt›+‹L›", L"取消，什么都不插", L""}},
	L"输入法",
	{L"进框时自动切成英文，打公式不用手动切输入法；框里要打中文时按 ‹Shift› 切换。",
	 L"上屏或取消后回到原输入框，原来是中文就自动切回中文。"},
	L"框的操作",
	{L"框的大小跟着内容变，太长会自动换行往下长。", L"按住框里文字以外的地方可以拖动。", L"跟随系统深色 / 浅色主题。"},
	L"托盘与安全",
	{L"程序在任务栏右下角托盘里运行（紫色 ∑ 图标），右键可以开关「开机自启」，或者「退出」。",
	 L"紧急强制退出：‹Win›+‹Alt›+‹L›，托盘点不动时也能用。",
	 L"程序会自我监控，内存或 CPU 异常时自动重启；万一崩溃也会在 1 秒内自动恢复。"},
	L"下次启动时不再显示", L"开始使用",
	L"语言", L"跟随系统", L"简体中文",
};

const Strings kZhTW = {
	L"zh-TW", false,
	L"使用說明", L"開機時啟動", L"結束", L"LatexAnywhere：Alt+L 輸入公式", L"Alt+L 已被其他程式佔用，暫時無法叫出公式框。",
	L"LatexAnywhere 使用說明",
	L"LatexAnywhere 使用說明",
	L"在任何軟體的輸入框裡，用 Obsidian + Latex Suite 的方式輸入公式。",
	L"基本用法",
	{L"在任何輸入框裡（LINE、Word、瀏覽器、記事本……）按 ‹Alt›+‹L›，游標旁會彈出公式框。",
	 L"直接輸入公式。框裡就是原版 Latex Suite：例如 ‹x^2› 會自動變成 ‹x^{2}›，‹//› 產生分式，‹Tab› 跳到下一個空位。",
	 L"框的上方即時顯示排版好的公式。",
	 L"用下列任一種方式送出，公式就會插回原本的輸入框。"},
	L"先選取一段文字再按 ‹Alt›+‹L›，選取內容會帶進框裡，修改後送出會取代原本的選取範圍。",
	L"送出方式", L"按鍵", L"插入的內容", L"適用於",
	{{L"‹Enter›", L"公式本身 ‹x^{2}›", L"LaTeX 原始碼、Overleaf"},
	 {L"‹Alt›+‹Enter›", L"含 $ 的行內公式 ‹$x^{2}$›", L"Typora、Obsidian、Markdown"},
	 {L"‹Ctrl›+‹Enter›", L"Unicode 字元 ‹x²›", L"聊天軟體等無法顯示公式的地方"},
	 {L"‹Ctrl›+‹Shift›+‹Enter›", L"Word 原生方程式（可點兩下編輯）", L"Word、PowerPoint"},
	 {L"‹Shift›+‹Enter›", L"在框裡換行", L"多行公式"},
	 {L"‹Esc› 或再按 ‹Alt›+‹L›", L"取消，什麼都不插入", L""}},
	L"輸入法",
	{L"進入框時自動切成英文，輸入公式不必手動切換；框裡要打中文時按 ‹Shift› 切換。",
	 L"送出或取消後回到原本的輸入框，原本是中文就會自動切回中文。"},
	L"框的操作",
	{L"框的大小隨內容變化，太長時會自動換行往下延伸。", L"按住框內文字以外的地方即可拖曳。", L"跟隨系統的深色／淺色主題。"},
	L"系統匣與安全",
	{L"程式在工作列右下角的系統匣執行（紫色 ∑ 圖示），按右鍵可切換「開機時啟動」或「結束」。",
	 L"緊急強制結束：‹Win›+‹Alt›+‹L›，系統匣沒有反應時也能用。",
	 L"程式會自我監控，記憶體或 CPU 異常時自動重新啟動；萬一當掉也會在 1 秒內自動恢復。"},
	L"下次啟動時不再顯示", L"開始使用",
	L"語言", L"跟隨系統", L"繁體中文",
};

const Strings kJa = {
	L"ja", false,
	L"使い方", L"Windows 起動時に開始", L"終了", L"LatexAnywhere：Alt+L で数式入力", L"Alt+L は他のアプリが使用中のため、数式ボックスを開けません。",
	L"LatexAnywhere の使い方",
	L"LatexAnywhere の使い方",
	L"どのアプリの入力欄でも、Obsidian + Latex Suite と同じ方法で数式を入力できます。",
	L"基本の使い方",
	{L"任意の入力欄（チャット、Word、ブラウザー、メモ帳など）で ‹Alt›+‹L› を押すと、カーソルの横に数式ボックスが開きます。",
	 L"そのまま数式を入力します。中身はオリジナルの Latex Suite です：‹x^2› は自動で ‹x^{2}› に、‹//› で分数、‹Tab› で次の入力位置へ移動します。",
	 L"ボックスの上部に、組版された数式がリアルタイムで表示されます。",
	 L"下のいずれかの方法で確定すると、元の入力欄に数式が挿入されます。"},
	L"文字列を選択してから ‹Alt›+‹L› を押すと、選択内容がボックスに読み込まれ、確定すると元の選択範囲が置き換わります。",
	L"確定の方法", L"キー", L"挿入される内容", L"向いている用途",
	{{L"‹Enter›", L"数式そのもの ‹x^{2}›", L"LaTeX ソース、Overleaf"},
	 {L"‹Alt›+‹Enter›", L"$ 付きのインライン数式 ‹$x^{2}$›", L"Typora、Obsidian、Markdown"},
	 {L"‹Ctrl›+‹Enter›", L"Unicode 文字 ‹x²›", L"数式を表示できないチャットなど"},
	 {L"‹Ctrl›+‹Shift›+‹Enter›", L"Word のネイティブ数式（ダブルクリックで編集可）", L"Word、PowerPoint"},
	 {L"‹Shift›+‹Enter›", L"ボックス内で改行", L"複数行の数式"},
	 {L"‹Esc› またはもう一度 ‹Alt›+‹L›", L"キャンセル（何も挿入しない）", L""}},
	L"IME（日本語入力）",
	{L"ボックスを開くと自動で英数入力に切り替わります。ボックス内で日本語を入力したいときは IME を切り替えてください。",
	 L"確定またはキャンセルして元の入力欄に戻ると、元が日本語入力なら自動で日本語入力に戻ります。"},
	L"ボックスの操作",
	{L"ボックスの大きさは内容に合わせて変わり、長い式は自動で折り返します。", L"文字以外の部分をドラッグすると移動できます。", L"システムのダーク／ライトテーマに従います。"},
	L"タスクトレイと安全機能",
	{L"タスクトレイ（紫の ∑ アイコン）で動作します。右クリックで「Windows 起動時に開始」の切り替えや「終了」ができます。",
	 L"緊急終了：‹Win›+‹Alt›+‹L›。トレイが反応しないときも使えます。",
	 L"メモリや CPU の異常を自己監視し、異常時は自動で再起動します。万一クラッシュしても 1 秒以内に復帰します。"},
	L"次回から表示しない", L"使い始める",
	L"言語", L"システムに合わせる", L"日本語",
};

const Strings kEn = {
	L"en", false,
	L"Help", L"Start with Windows", L"Quit", L"LatexAnywhere: Alt+L to type math", L"Alt+L is used by another program, so the formula box cannot open.",
	L"LatexAnywhere Help",
	L"LatexAnywhere Help",
	L"Type math in any text box, the Obsidian + Latex Suite way.",
	L"Basics",
	{L"In any text box (chat apps, Word, browsers, Notepad…) press ‹Alt›+‹L›; the formula box opens next to the cursor.",
	 L"Type your formula. The box runs the original Latex Suite: ‹x^2› becomes ‹x^{2}›, ‹//› makes a fraction, ‹Tab› jumps to the next placeholder.",
	 L"The rendered formula is shown live at the top of the box.",
	 L"Insert it with any of the keys below; it lands where your cursor was."},
	L"Select text first and press ‹Alt›+‹L› to edit it; inserting replaces the selection.",
	L"Insert", L"Key", L"Inserts", L"Good for",
	{{L"‹Enter›", L"the formula ‹x^{2}›", L"LaTeX sources, Overleaf"},
	 {L"‹Alt›+‹Enter›", L"inline math with $: ‹$x^{2}$›", L"Typora, Obsidian, Markdown"},
	 {L"‹Ctrl›+‹Enter›", L"Unicode characters ‹x²›", L"chat apps without math rendering"},
	 {L"‹Ctrl›+‹Shift›+‹Enter›", L"a native Word equation (editable)", L"Word, PowerPoint"},
	 {L"‹Shift›+‹Enter›", L"a new line in the box", L"multi-line formulas"},
	 {L"‹Esc› or ‹Alt›+‹L› again", L"cancel, insert nothing", L""}},
	L"Input methods",
	{L"The box switches your input method to English automatically; switch it back inside the box if you need to type other scripts.",
	 L"Back in your text box, the input method returns to its previous language."},
	L"The box",
	{L"It grows with its content and wraps long lines.", L"Drag it by any area outside the text.", L"It follows the system dark / light theme."},
	L"Tray and safety",
	{L"It runs in the system tray (purple ∑ icon). Right-click to toggle Start with Windows or to Quit.",
	 L"Emergency quit: ‹Win›+‹Alt›+‹L›, even if the tray does not respond.",
	 L"It watches its own memory and CPU and restarts itself if anything goes wrong; a crash recovers within a second."},
	L"Don't show this at startup", L"Start",
	L"Language", L"Follow system", L"English",
};

const Strings kKo = {
	L"ko", false,
	L"사용법", L"Windows 시작 시 실행", L"종료", L"LatexAnywhere: Alt+L로 수식 입력", L"다른 프로그램이 Alt+L을 사용 중이라 수식 상자를 열 수 없습니다.",
	L"LatexAnywhere 사용법",
	L"LatexAnywhere 사용법",
	L"어떤 프로그램의 입력란에서든 Obsidian + Latex Suite 방식으로 수식을 입력합니다.",
	L"기본 사용법",
	{L"아무 입력란(메신저, Word, 브라우저, 메모장 등)에서 ‹Alt›+‹L›을 누르면 커서 옆에 수식 상자가 열립니다.",
	 L"바로 수식을 입력하세요. 상자 안은 원본 Latex Suite입니다: ‹x^2›는 자동으로 ‹x^{2}›가 되고, ‹//›는 분수, ‹Tab›은 다음 빈칸으로 이동합니다.",
	 L"상자 위쪽에 조판된 수식이 실시간으로 표시됩니다.",
	 L"아래 방법 중 하나로 입력하면 원래 입력란에 수식이 삽입됩니다."},
	L"텍스트를 먼저 선택하고 ‹Alt›+‹L›을 누르면 선택한 내용을 편집할 수 있고, 입력하면 원래 선택 영역을 바꿉니다.",
	L"입력 방법", L"키", L"삽입되는 내용", L"용도",
	{{L"‹Enter›", L"수식 자체 ‹x^{2}›", L"LaTeX 소스, Overleaf"},
	 {L"‹Alt›+‹Enter›", L"$가 붙은 인라인 수식 ‹$x^{2}$›", L"Typora, Obsidian, Markdown"},
	 {L"‹Ctrl›+‹Enter›", L"유니코드 문자 ‹x²›", L"수식을 표시하지 못하는 메신저 등"},
	 {L"‹Ctrl›+‹Shift›+‹Enter›", L"Word 기본 수식(두 번 클릭해 편집 가능)", L"Word, PowerPoint"},
	 {L"‹Shift›+‹Enter›", L"상자 안에서 줄 바꿈", L"여러 줄 수식"},
	 {L"‹Esc› 또는 ‹Alt›+‹L› 다시 누르기", L"취소, 아무것도 삽입하지 않음", L""}},
	L"입력기",
	{L"상자를 열면 자동으로 영문 입력으로 바뀝니다. 상자 안에서 한글을 입력하려면 한/영 키로 전환하세요.",
	 L"입력하거나 취소하고 원래 입력란으로 돌아가면, 원래 한글 입력이었을 경우 자동으로 한글로 돌아갑니다."},
	L"상자 조작",
	{L"상자 크기는 내용에 맞춰 바뀌고, 긴 수식은 자동으로 줄 바꿈됩니다.", L"텍스트 이외의 부분을 끌어서 이동할 수 있습니다.", L"시스템의 다크/라이트 테마를 따릅니다."},
	L"트레이와 안전",
	{L"작업 표시줄 트레이(보라색 ∑ 아이콘)에서 실행됩니다. 오른쪽 클릭으로 'Windows 시작 시 실행' 전환이나 '종료'를 할 수 있습니다.",
	 L"긴급 강제 종료: ‹Win›+‹Alt›+‹L›. 트레이가 응답하지 않을 때도 사용할 수 있습니다.",
	 L"메모리와 CPU를 스스로 감시하여 이상 시 자동으로 다시 시작하며, 충돌해도 1초 안에 복구됩니다."},
	L"다음부터 시작할 때 표시하지 않음", L"시작하기",
	L"언어", L"시스템 설정 따르기", L"한국어",
};

const Strings kVi = {
	L"vi", false,
	L"Hướng dẫn", L"Khởi động cùng Windows", L"Thoát", L"LatexAnywhere: Alt+L để gõ công thức", L"Alt+L đang được chương trình khác sử dụng nên không mở được hộp công thức.",
	L"Hướng dẫn LatexAnywhere",
	L"Hướng dẫn LatexAnywhere",
	L"Gõ công thức toán trong mọi ô nhập liệu theo cách của Obsidian + Latex Suite.",
	L"Cách dùng cơ bản",
	{L"Trong bất kỳ ô nhập liệu nào (ứng dụng chat, Word, trình duyệt, Notepad…) nhấn ‹Alt›+‹L›, hộp công thức sẽ mở cạnh con trỏ.",
	 L"Gõ công thức trực tiếp. Bên trong là Latex Suite gốc: ‹x^2› tự thành ‹x^{2}›, ‹//› tạo phân số, ‹Tab› nhảy tới chỗ trống tiếp theo.",
	 L"Phía trên hộp hiển thị công thức đã dàn trang theo thời gian thực.",
	 L"Chèn bằng một trong các phím dưới đây; công thức sẽ được đưa về đúng ô nhập liệu ban đầu."},
	L"Chọn văn bản trước rồi nhấn ‹Alt›+‹L› để sửa; khi chèn sẽ thay thế đoạn đã chọn.",
	L"Cách chèn", L"Phím", L"Nội dung chèn", L"Phù hợp với",
	{{L"‹Enter›", L"chính công thức ‹x^{2}›", L"mã nguồn LaTeX, Overleaf"},
	 {L"‹Alt›+‹Enter›", L"công thức nội dòng có $: ‹$x^{2}$›", L"Typora, Obsidian, Markdown"},
	 {L"‹Ctrl›+‹Enter›", L"ký tự Unicode ‹x²›", L"ứng dụng chat không hiển thị công thức"},
	 {L"‹Ctrl›+‹Shift›+‹Enter›", L"phương trình gốc của Word (sửa được)", L"Word, PowerPoint"},
	 {L"‹Shift›+‹Enter›", L"xuống dòng trong hộp", L"công thức nhiều dòng"},
	 {L"‹Esc› hoặc nhấn lại ‹Alt›+‹L›", L"hủy, không chèn gì", L""}},
	L"Bộ gõ",
	{L"Khi mở hộp, bộ gõ tự chuyển sang tiếng Anh; nếu cần gõ tiếng Việt trong hộp hãy chuyển lại bộ gõ.",
	 L"Sau khi chèn hoặc hủy, quay lại ô nhập liệu ban đầu thì bộ gõ trở về ngôn ngữ trước đó."},
	L"Thao tác với hộp",
	{L"Hộp tự đổi kích thước theo nội dung và tự xuống dòng khi quá dài.", L"Kéo vùng bên ngoài chữ để di chuyển hộp.", L"Theo giao diện sáng / tối của hệ thống."},
	L"Khay hệ thống và an toàn",
	{L"Chương trình chạy ở khay hệ thống (biểu tượng ∑ màu tím). Nhấp chuột phải để bật/tắt Khởi động cùng Windows hoặc Thoát.",
	 L"Thoát khẩn cấp: ‹Win›+‹Alt›+‹L›, dùng được cả khi khay không phản hồi.",
	 L"Chương trình tự giám sát bộ nhớ và CPU, tự khởi động lại khi bất thường; nếu bị lỗi cũng tự phục hồi trong 1 giây."},
	L"Không hiển thị lại khi khởi động", L"Bắt đầu",
	L"Ngôn ngữ", L"Theo hệ thống", L"Tiếng Việt",
};

const Strings kRu = {
	L"ru", false,
	L"Справка", L"Запускать вместе с Windows", L"Выход", L"LatexAnywhere: Alt+L — ввод формулы", L"Сочетание Alt+L занято другой программой, окно формулы открыть нельзя.",
	L"Справка LatexAnywhere",
	L"Справка LatexAnywhere",
	L"Набирайте формулы в любом поле ввода так же, как в Obsidian + Latex Suite.",
	L"Основы",
	{L"В любом поле ввода (мессенджер, Word, браузер, Блокнот…) нажмите ‹Alt›+‹L› — рядом с курсором откроется окно формулы.",
	 L"Набирайте формулу. Внутри работает оригинальный Latex Suite: ‹x^2› превращается в ‹x^{2}›, ‹//› создаёт дробь, ‹Tab› переходит к следующему полю.",
	 L"Сверху окна формула отображается в свёрстанном виде в реальном времени.",
	 L"Вставьте её любой из клавиш ниже — формула появится там, где был курсор."},
	L"Выделите текст и нажмите ‹Alt›+‹L›, чтобы отредактировать его; вставка заменит выделение.",
	L"Вставка", L"Клавиши", L"Что вставляется", L"Подходит для",
	{{L"‹Enter›", L"сама формула ‹x^{2}›", L"исходники LaTeX, Overleaf"},
	 {L"‹Alt›+‹Enter›", L"строчная формула с $: ‹$x^{2}$›", L"Typora, Obsidian, Markdown"},
	 {L"‹Ctrl›+‹Enter›", L"символы Unicode ‹x²›", L"мессенджеры без поддержки формул"},
	 {L"‹Ctrl›+‹Shift›+‹Enter›", L"родное уравнение Word (можно редактировать)", L"Word, PowerPoint"},
	 {L"‹Shift›+‹Enter›", L"новая строка в окне", L"многострочные формулы"},
	 {L"‹Esc› или снова ‹Alt›+‹L›", L"отмена, ничего не вставляется", L""}},
	L"Раскладка клавиатуры",
	{L"При открытии окна раскладка автоматически переключается на английскую; для ввода русского текста переключите её вручную.",
	 L"После вставки или отмены прежняя раскладка восстанавливается."},
	L"Окно формулы",
	{L"Окно подстраивается под содержимое и переносит длинные строки.", L"Перетаскивайте окно за любую область вне текста.", L"Следует тёмной или светлой теме системы."},
	L"Трей и надёжность",
	{L"Программа работает в области уведомлений (фиолетовый значок ∑). Правый клик — включить автозапуск или выйти.",
	 L"Аварийный выход: ‹Win›+‹Alt›+‹L› — работает, даже если трей не отвечает.",
	 L"Программа следит за своей памятью и процессором и сама перезапускается при сбое; после аварии восстанавливается за секунду."},
	L"Больше не показывать при запуске", L"Начать",
	L"Язык", L"Как в системе", L"Русский",
};

const Strings kAr = {
	L"ar", true,
	L"التعليمات", L"التشغيل مع Windows", L"خروج", L"LatexAnywhere: ‏Alt+L لكتابة المعادلات", L"اختصار Alt+L يستخدمه برنامج آخر، لذا لا يمكن فتح مربع المعادلة.",
	L"تعليمات LatexAnywhere",
	L"تعليمات LatexAnywhere",
	L"اكتب المعادلات الرياضية في أي حقل نصي بطريقة Obsidian + Latex Suite.",
	L"الأساسيات",
	{L"في أي حقل نصي (تطبيقات الدردشة، Word، المتصفح، المفكرة…) اضغط ‹Alt›+‹L› ليظهر مربع المعادلة بجوار المؤشر.",
	 L"اكتب المعادلة مباشرة. يعمل داخل المربع Latex Suite الأصلي: ‹x^2› تصبح ‹x^{2}›، و‹//› ينشئ كسرًا، و‹Tab› ينتقل إلى الخانة التالية.",
	 L"تظهر المعادلة منسقة أعلى المربع لحظة بلحظة.",
	 L"أدرجها بأي من المفاتيح أدناه لتعود إلى موضع المؤشر في الحقل الأصلي."},
	L"حدّد نصًا أولًا ثم اضغط ‹Alt›+‹L› لتحريره؛ عند الإدراج يحل محل التحديد.",
	L"طرق الإدراج", L"المفاتيح", L"ما يُدرج", L"مناسب لـ",
	{{L"‹Enter›", L"المعادلة نفسها ‹x^{2}›", L"ملفات LaTeX، ‏Overleaf"},
	 {L"‹Alt›+‹Enter›", L"معادلة سطرية مع $: ‹$x^{2}$›", L"Typora، ‏Obsidian، ‏Markdown"},
	 {L"‹Ctrl›+‹Enter›", L"رموز Unicode ‏‹x²›", L"تطبيقات الدردشة التي لا تعرض المعادلات"},
	 {L"‹Ctrl›+‹Shift›+‹Enter›", L"معادلة Word أصلية (قابلة للتحرير)", L"Word، ‏PowerPoint"},
	 {L"‹Shift›+‹Enter›", L"سطر جديد داخل المربع", L"المعادلات متعددة الأسطر"},
	 {L"‹Esc› أو ‹Alt›+‹L› مرة أخرى", L"إلغاء دون إدراج", L""}},
	L"لغة الإدخال",
	{L"عند فتح المربع تتحول لغة الإدخال إلى الإنجليزية تلقائيًا؛ بدّلها يدويًا إذا أردت الكتابة بالعربية داخله.",
	 L"بعد الإدراج أو الإلغاء تعود لغة الإدخال السابقة في الحقل الأصلي."},
	L"مربع المعادلة",
	{L"يتغير حجمه مع المحتوى ويلفّ الأسطر الطويلة تلقائيًا.", L"اسحبه من أي منطقة خارج النص.", L"يتبع السمة الداكنة أو الفاتحة للنظام."},
	L"علبة النظام والأمان",
	{L"يعمل البرنامج في علبة النظام (أيقونة ∑ بنفسجية). انقر بزر الفأرة الأيمن لتفعيل التشغيل مع Windows أو للخروج.",
	 L"الخروج الطارئ: ‹Win›+‹Alt›+‹L›، يعمل حتى إن لم تستجب العلبة.",
	 L"يراقب البرنامج ذاكرته ومعالجه ويعيد تشغيل نفسه عند الخلل، ويتعافى من الأعطال خلال ثانية."},
	L"عدم الإظهار عند بدء التشغيل مرة أخرى", L"ابدأ",
	L"اللغة", L"حسب لغة النظام", L"العربية",
};

const Strings kEs = {
	L"es", false,
	L"Ayuda", L"Iniciar con Windows", L"Salir", L"LatexAnywhere: Alt+L para escribir fórmulas", L"Alt+L está en uso por otro programa; no se puede abrir el cuadro de fórmulas.",
	L"Ayuda de LatexAnywhere",
	L"Ayuda de LatexAnywhere",
	L"Escribe fórmulas en cualquier cuadro de texto al estilo de Obsidian + Latex Suite.",
	L"Lo básico",
	{L"En cualquier cuadro de texto (chats, Word, navegadores, Bloc de notas…) pulsa ‹Alt›+‹L›; el cuadro de fórmulas se abre junto al cursor.",
	 L"Escribe la fórmula. Dentro funciona el Latex Suite original: ‹x^2› se convierte en ‹x^{2}›, ‹//› crea una fracción y ‹Tab› salta al siguiente hueco.",
	 L"La fórmula compuesta se muestra en tiempo real en la parte superior.",
	 L"Insértala con cualquiera de las teclas de abajo; aparecerá donde estaba el cursor."},
	L"Selecciona texto antes de pulsar ‹Alt›+‹L› para editarlo; al insertar se reemplaza la selección.",
	L"Inserción", L"Teclas", L"Inserta", L"Ideal para",
	{{L"‹Enter›", L"la fórmula ‹x^{2}›", L"código LaTeX, Overleaf"},
	 {L"‹Alt›+‹Enter›", L"fórmula en línea con $: ‹$x^{2}$›", L"Typora, Obsidian, Markdown"},
	 {L"‹Ctrl›+‹Enter›", L"caracteres Unicode ‹x²›", L"chats que no muestran fórmulas"},
	 {L"‹Ctrl›+‹Shift›+‹Enter›", L"ecuación nativa de Word (editable)", L"Word, PowerPoint"},
	 {L"‹Shift›+‹Enter›", L"salto de línea en el cuadro", L"fórmulas de varias líneas"},
	 {L"‹Esc› o ‹Alt›+‹L› de nuevo", L"cancelar sin insertar nada", L""}},
	L"Método de entrada",
	{L"Al abrir el cuadro el método de entrada pasa automáticamente a inglés; cámbialo dentro del cuadro si necesitas otro idioma.",
	 L"Al insertar o cancelar, el cuadro de texto original recupera el método de entrada anterior."},
	L"El cuadro",
	{L"Crece con el contenido y ajusta las líneas largas.", L"Arrástralo desde cualquier zona fuera del texto.", L"Sigue el tema oscuro o claro del sistema."},
	L"Bandeja y seguridad",
	{L"Funciona en la bandeja del sistema (icono ∑ morado). Clic derecho para activar Iniciar con Windows o Salir.",
	 L"Salida de emergencia: ‹Win›+‹Alt›+‹L›, incluso si la bandeja no responde.",
	 L"Vigila su propia memoria y CPU y se reinicia si algo falla; tras un cierre inesperado se recupera en un segundo."},
	L"No volver a mostrar al iniciar", L"Empezar",
	L"Idioma", L"Según el sistema", L"Español",
};

const Strings kPt = {
	L"pt-BR", false,
	L"Ajuda", L"Iniciar com o Windows", L"Sair", L"LatexAnywhere: Alt+L para digitar fórmulas", L"Alt+L está sendo usado por outro programa; não é possível abrir a caixa de fórmulas.",
	L"Ajuda do LatexAnywhere",
	L"Ajuda do LatexAnywhere",
	L"Digite fórmulas em qualquer caixa de texto do jeito do Obsidian + Latex Suite.",
	L"O básico",
	{L"Em qualquer caixa de texto (apps de conversa, Word, navegadores, Bloco de Notas…) pressione ‹Alt›+‹L›; a caixa de fórmulas abre ao lado do cursor.",
	 L"Digite a fórmula. Dentro roda o Latex Suite original: ‹x^2› vira ‹x^{2}›, ‹//› cria uma fração e ‹Tab› pula para o próximo espaço.",
	 L"A fórmula diagramada aparece em tempo real no topo da caixa.",
	 L"Insira com qualquer uma das teclas abaixo; ela vai para onde estava o cursor."},
	L"Selecione um texto antes de pressionar ‹Alt›+‹L› para editá-lo; ao inserir, a seleção é substituída.",
	L"Inserção", L"Teclas", L"Insere", L"Bom para",
	{{L"‹Enter›", L"a fórmula ‹x^{2}›", L"código LaTeX, Overleaf"},
	 {L"‹Alt›+‹Enter›", L"fórmula em linha com $: ‹$x^{2}$›", L"Typora, Obsidian, Markdown"},
	 {L"‹Ctrl›+‹Enter›", L"caracteres Unicode ‹x²›", L"apps de conversa sem fórmulas"},
	 {L"‹Ctrl›+‹Shift›+‹Enter›", L"equação nativa do Word (editável)", L"Word, PowerPoint"},
	 {L"‹Shift›+‹Enter›", L"nova linha na caixa", L"fórmulas de várias linhas"},
	 {L"‹Esc› ou ‹Alt›+‹L› de novo", L"cancelar sem inserir nada", L""}},
	L"Método de entrada",
	{L"Ao abrir a caixa, o método de entrada muda automaticamente para inglês; troque dentro da caixa se precisar de outro idioma.",
	 L"Ao inserir ou cancelar, a caixa de texto original volta ao método de entrada anterior."},
	L"A caixa",
	{L"Cresce com o conteúdo e quebra linhas longas.", L"Arraste por qualquer área fora do texto.", L"Segue o tema escuro ou claro do sistema."},
	L"Bandeja e segurança",
	{L"Funciona na bandeja do sistema (ícone ∑ roxo). Clique com o botão direito para ativar Iniciar com o Windows ou Sair.",
	 L"Saída de emergência: ‹Win›+‹Alt›+‹L›, mesmo se a bandeja não responder.",
	 L"Monitora a própria memória e CPU e se reinicia se algo der errado; após uma falha, volta em um segundo."},
	L"Não mostrar novamente ao iniciar", L"Começar",
	L"Idioma", L"Seguir o sistema", L"Português (Brasil)",
};

const Strings* Pick() {
	LANGID id = GetUserDefaultUILanguage();
	switch (PRIMARYLANGID(id)) {
	case LANG_CHINESE: {
		WORD sub = SUBLANGID(id);
		return (sub == SUBLANG_CHINESE_TRADITIONAL || sub == SUBLANG_CHINESE_HONGKONG || sub == SUBLANG_CHINESE_MACAU) ? &kZhTW : &kZhCN;
	}
	case LANG_JAPANESE: return &kJa;
	case LANG_KOREAN: return &kKo;
	case LANG_VIETNAMESE: return &kVi;
	case LANG_RUSSIAN: return &kRu;
	case LANG_ARABIC: return &kAr;
	case LANG_SPANISH: return &kEs;
	case LANG_PORTUGUESE: return &kPt;
	default: return &kEn;
	}
}

}  // namespace

const Strings* const kAll[] = {&kZhCN, &kZhTW, &kJa, &kEn, &kKo, &kVi, &kRu, &kAr, &kEs, &kPt};
const Strings* g_current = nullptr;
wchar_t g_setting[16] = L"";   // 空 = 跟随系统
constexpr wchar_t kRegKey[] = L"Software\\LatexAnywhere";

const Strings* ByCode(const wchar_t* code) {
	for (const Strings* c : kAll)
		if (!lstrcmpiW(c->code, code)) return c;
	return nullptr;
}

void Resolve() {
	const Strings* s = nullptr;
	wchar_t v[16];
	if (GetEnvironmentVariableW(L"LA_LANG", v, 16)) s = ByCode(v);   // 测试用：强制指定语言
	if (!s && g_setting[0]) s = ByCode(g_setting);
	g_current = s ? s : Pick();
}

const Strings& Lang() {
	if (!g_current) {
		DWORD n = sizeof g_setting;
		if (RegGetValueW(HKEY_CURRENT_USER, kRegKey, L"Language", RRF_RT_REG_SZ, nullptr, g_setting, &n) != ERROR_SUCCESS) g_setting[0] = 0;
		Resolve();
	}
	return *g_current;
}

void SetLanguage(const wchar_t* code) {
	lstrcpynW(g_setting, code ? code : L"", 16);
	HKEY k;
	if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
		RegSetValueExW(k, L"Language", 0, REG_SZ, reinterpret_cast<const BYTE*>(g_setting), (DWORD)((lstrlenW(g_setting) + 1) * sizeof(wchar_t)));
		RegCloseKey(k);
	}
	Resolve();
}

const wchar_t* LanguageSetting() { Lang(); return g_setting; }
int LanguageCount() { return (int)(sizeof kAll / sizeof kAll[0]); }
const Strings& LanguageAt(int i) { return *kAll[i]; }

}  // namespace la
