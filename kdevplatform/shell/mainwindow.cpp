/*
    SPDX-FileCopyrightText: 2002 Falk Brettschneider <falkbr@kdevelop.org>
    SPDX-FileCopyrightText: 2003 John Firebaugh <jfirebaugh@kde.org>
    SPDX-FileCopyrightText: 2006 Adam Treat <treat@kde.org>
    SPDX-FileCopyrightText: 2006, 2007 Alexander Dymo <adymo@kdevelop.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "mainwindow.h"
#include "mainwindow_p.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDockWidget>
#include <QDomElement>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPalette>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>

#include <KActionCollection>
#include <KLocalizedString>
#include <KShortcutsDialog>
#include <KTextEditor/Document>
#include <KTextEditor/View>
#include <KWindowSystem>
#include <KXMLGUIFactory>

#define HAVE_X11 __has_include(<KX11Extras>)
#if HAVE_X11
#include <KX11Extras>
#endif

#include <sublime/area.h>
#include "shellextension.h"
#include "partcontroller.h"
#include "plugincontroller.h"
#include "projectcontroller.h"
#include "uicontroller.h"
#include "documentcontroller.h"
#include "workingsetcontroller.h"
#include "sessioncontroller.h"
#include "sourceformattercontroller.h"
#include "areadisplay.h"
#include "project.h"
#include "debug.h"
#include "uiconfig.h"
#include "ktexteditorpluginintegration.h"

#include <interfaces/isession.h>
#include <interfaces/iprojectcontroller.h>
#include <sublime/view.h>
#include <sublime/document.h>
#include <sublime/urldocument.h>
#include <sublime/container.h>
#include <util/path.h>
#include <util/widgetcolorizer.h>

using namespace KDevelop;

namespace {

QColor defaultColor(const QPalette& palette)
{
    return palette.windowText().color();
}

bool rriseIsDarkWindowPalette(const QPalette& palette)
{
    return palette.color(QPalette::Window).lightness() < 128;
}

bool rriseIsDarkPalette()
{
    return rriseIsDarkWindowPalette(QApplication::palette());
}

void rriseSetPaletteColor(QPalette& palette, QPalette::ColorRole role, const QColor& active, const QColor& disabled)
{
    palette.setColor(QPalette::Active, role, active);
    palette.setColor(QPalette::Inactive, role, active);
    palette.setColor(QPalette::Disabled, role, disabled);
}

QString rriseDarkMenuStyleSheet()
{
    return QStringLiteral(
        "QMenu {"
        "background: #252526;"
        "color: #d4d4d4;"
        "border: 1px solid #3c3c3c;"
        "}"
        "QMenu::section {"
        "background: #252526;"
        "color: #ffffff;"
        "font-weight: 600;"
        "padding: 5px 26px 5px 24px;"
        "}"
        "QMenu::item {"
        "background: transparent;"
        "color: #d4d4d4;"
        "padding: 5px 26px 5px 24px;"
        "}"
        "QMenu::item:selected {"
        "background: #264f78;"
        "color: #ffffff;"
        "}"
        "QMenu::item:disabled {"
        "color: #858585;"
        "}"
        "QMenu::item:checked {"
        "background: #2a2d2e;"
        "color: #ffffff;"
        "}"
        "QMenu::item:checked:selected {"
        "background: #264f78;"
        "color: #ffffff;"
        "}"
        "QMenu::separator {"
        "height: 1px;"
        "background: #3c3c3c;"
        "margin: 4px 8px;"
        "}"
        "QMenu::right-arrow {"
        "image: none;"
        "width: 6px;"
        "height: 6px;"
        "}");
}

QString rriseDarkMenuBarStyleSheet()
{
    return QStringLiteral(
        "QMenuBar { color: #d4d4d4; }"
        "QMenuBar::item { color: #d4d4d4; background: transparent; padding: 4px 8px; }"
        "QMenuBar::item:selected, QMenuBar::item:pressed { color: #ffffff; background: #2a2d2e; }"
        "QMenuBar::item:disabled { color: #858585; }");
}

QString rriseDarkToolBarStyleSheet()
{
    return QStringLiteral(
        "QToolBar { color: #d4d4d4; }"
        "QToolButton { color: #d4d4d4; }"
        "QToolButton:disabled { color: #858585; }"
        "QToolButton::menu-indicator { color: #d4d4d4; }");
}

QString rriseDarkToolButtonStyleSheet()
{
    return QStringLiteral(
        "QToolButton { color: #d4d4d4; }"
        "QToolButton:disabled { color: #858585; }"
        "QToolButton::menu-indicator { color: #d4d4d4; }");
}

void rriseApplyReadableDarkTextPalette(QWidget* widget, bool dark, const QString& darkStyleSheet = QString{})
{
    if (!widget) {
        return;
    }

    if (!dark) {
        widget->setPalette(QApplication::palette(widget));
        if (!widget->styleSheet().isEmpty()) {
            widget->setStyleSheet(QString{});
        }
        return;
    }

    QPalette palette = widget->palette();
    const QColor textColor(QStringLiteral("#d4d4d4"));
    const QColor disabledTextColor(QStringLiteral("#858585"));
    const QColor backgroundColor(QStringLiteral("#252526"));
    const QColor baseColor(QStringLiteral("#1e1e1e"));
    const QColor highlightColor(QStringLiteral("#264f78"));
    rriseSetPaletteColor(palette, QPalette::WindowText, textColor, disabledTextColor);
    rriseSetPaletteColor(palette, QPalette::Text, textColor, disabledTextColor);
    rriseSetPaletteColor(palette, QPalette::ButtonText, textColor, disabledTextColor);
    rriseSetPaletteColor(palette, QPalette::ToolTipText, QColor(QStringLiteral("#ffffff")), disabledTextColor);
    rriseSetPaletteColor(palette, QPalette::Button, backgroundColor, backgroundColor);
    rriseSetPaletteColor(palette, QPalette::Window, backgroundColor, backgroundColor);
    rriseSetPaletteColor(palette, QPalette::Base, baseColor, baseColor);
    rriseSetPaletteColor(palette, QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")), disabledTextColor);
    rriseSetPaletteColor(palette, QPalette::Highlight, highlightColor, highlightColor);
    palette.setColor(QPalette::PlaceholderText, disabledTextColor);
    widget->setPalette(palette);
    if (widget->styleSheet() != darkStyleSheet) {
        widget->setStyleSheet(darkStyleSheet);
    }
}

void rriseApplyReadableDarkMenu(QMenu* menu)
{
    if (!menu) {
        return;
    }

    rriseApplyReadableDarkTextPalette(menu, rriseIsDarkPalette(), rriseDarkMenuStyleSheet());
    for (QAction* action : menu->actions()) {
        if (QMenu* subMenu = action->menu()) {
            rriseApplyReadableDarkMenu(subMenu);
        }
    }
}

QColor colorForDocument(const QUrl& url, const QPalette& palette, const QColor& defaultColor)
{
    auto project = Core::self()->projectController()->findProjectForUrl(url);
    if (!project)
        return defaultColor;

    return WidgetColorizer::colorForId(qHash(project->path()), palette);
}

QString normalizedMenuTitle(QString title)
{
    title.remove(QLatin1Char('&'));
    const int tabIndex = title.indexOf(QLatin1Char('\t'));
    if (tabIndex >= 0) {
        title.truncate(tabIndex);
    }
    title.replace(QChar(0x2026), QStringLiteral("..."));
    return title.trimmed();
}

const QHash<QString, QString>& menuTitleTranslations()
{
    static const QHash<QString, QString> translations = {
        {QStringLiteral("Session"), QStringLiteral("会话")},
        {QStringLiteral("Project"), QStringLiteral("工程")},
        {QStringLiteral("Run"), QStringLiteral("运行")},
        {QStringLiteral("Navigation"), QStringLiteral("导航")},
        {QStringLiteral("File"), QStringLiteral("文件")},
        {QStringLiteral("Edit"), QStringLiteral("编辑")},
        {QStringLiteral("View"), QStringLiteral("视图")},
        {QStringLiteral("Selection"), QStringLiteral("选择")},
        {QStringLiteral("Go"), QStringLiteral("转到")},
        {QStringLiteral("Tools"), QStringLiteral("工具")},
        {QStringLiteral("Code"), QStringLiteral("代码")},
        {QStringLiteral("Window"), QStringLiteral("窗口")},
        {QStringLiteral("Settings"), QStringLiteral("设置")},
        {QStringLiteral("Help"), QStringLiteral("帮助")},
        {QStringLiteral("Bookmarks"), QStringLiteral("书签")},
        {QStringLiteral("Spelling"), QStringLiteral("拼写检查")},
        {QStringLiteral("Mode"), QStringLiteral("模式")},
        {QStringLiteral("Scripts"), QStringLiteral("脚本")},
        {QStringLiteral("Highlighting"), QStringLiteral("语法高亮")},
        {QStringLiteral("Indentation"), QStringLiteral("缩进")},
        {QStringLiteral("End of Line"), QStringLiteral("行尾格式")},
        {QStringLiteral("Folding"), QStringLiteral("代码折叠")},
        {QStringLiteral("Word Wrap"), QStringLiteral("自动换行")},
        {QStringLiteral("View Mode"), QStringLiteral("视图模式")},
        {QStringLiteral("Color Scheme"), QStringLiteral("配色方案")},
        {QStringLiteral("Examine Core File"), QStringLiteral("检查 Core 文件")},
        {QStringLiteral("Attach to Process"), QStringLiteral("附加到进程")},
        {QStringLiteral("Analyze Current File With"), QStringLiteral("使用以下工具分析当前文件")},
        {QStringLiteral("Analyze Current Project With"), QStringLiteral("使用以下工具分析当前工程")},
    };
    return translations;
}

const QHash<QString, QString>& actionTextTranslations()
{
    static const QHash<QString, QString> translations = {
        {QStringLiteral("New"), QStringLiteral("新建")},
        {QStringLiteral("New..."), QStringLiteral("新建...")},
        {QStringLiteral("New File"), QStringLiteral("新建文件")},
        {QStringLiteral("New File..."), QStringLiteral("新建文件...")},
        {QStringLiteral("New from Template..."), QStringLiteral("从模板新建...")},
        {QStringLiteral("Open..."), QStringLiteral("打开...")},
        {QStringLiteral("Open / Import Project..."), QStringLiteral("打开/导入工程...")},
        {QStringLiteral("Open Recent"), QStringLiteral("打开最近使用")},
        {QStringLiteral("Open Recent Project"), QStringLiteral("打开最近工程")},
        {QStringLiteral("Open Project for Current File"), QStringLiteral("打开当前文件所属工程")},
        {QStringLiteral("Open Configuration..."), QStringLiteral("打开配置...")},
        {QStringLiteral("Fetch Project..."), QStringLiteral("获取工程...")},
        {QStringLiteral("Close"), QStringLiteral("关闭")},
        {QStringLiteral("Close All"), QStringLiteral("全部关闭")},
        {QStringLiteral("Close All Other"), QStringLiteral("关闭其他")},
        {QStringLiteral("Close All Others"), QStringLiteral("关闭其他")},
        {QStringLiteral("Close Project(s)"), QStringLiteral("关闭工程")},
        {QStringLiteral("Quit"), QStringLiteral("退出")},
        {QStringLiteral("Save"), QStringLiteral("保存")},
        {QStringLiteral("Save As..."), QStringLiteral("另存为...")},
        {QStringLiteral("Save All"), QStringLiteral("全部保存")},
        {QStringLiteral("Reload"), QStringLiteral("重新加载")},
        {QStringLiteral("Reload All"), QStringLiteral("全部重新加载")},
        {QStringLiteral("Print..."), QStringLiteral("打印...")},
        {QStringLiteral("Print Preview"), QStringLiteral("打印预览")},
        {QStringLiteral("Undo"), QStringLiteral("撤销")},
        {QStringLiteral("Redo"), QStringLiteral("重做")},
        {QStringLiteral("Cut"), QStringLiteral("剪切")},
        {QStringLiteral("Copy"), QStringLiteral("复制")},
        {QStringLiteral("Copy All"), QStringLiteral("全部复制")},
        {QStringLiteral("Copy Filename"), QStringLiteral("复制文件名")},
        {QStringLiteral("Paste"), QStringLiteral("粘贴")},
        {QStringLiteral("Select All"), QStringLiteral("全选")},
        {QStringLiteral("Deselect"), QStringLiteral("取消选择")},
        {QStringLiteral("Select to Matching Bracket"), QStringLiteral("选择到匹配括号")},
        {QStringLiteral("Toggle Block Selection"), QStringLiteral("切换块选择")},
        {QStringLiteral("Find..."), QStringLiteral("查找...")},
        {QStringLiteral("Find Next"), QStringLiteral("查找下一个")},
        {QStringLiteral("Find Previous"), QStringLiteral("查找上一个")},
        {QStringLiteral("Replace..."), QStringLiteral("替换...")},
        {QStringLiteral("Find/Replace in Files..."), QStringLiteral("在文件中查找/替换...")},
        {QStringLiteral("Insert Tab"), QStringLiteral("插入制表符")},
        {QStringLiteral("Delete Line"), QStringLiteral("删除当前行")},
        {QStringLiteral("Delete Word Left"), QStringLiteral("删除左侧单词")},
        {QStringLiteral("Delete Word Right"), QStringLiteral("删除右侧单词")},
        {QStringLiteral("Transpose Characters"), QStringLiteral("交换字符")},
        {QStringLiteral("Move Lines Up"), QStringLiteral("上移行")},
        {QStringLiteral("Move Lines Down"), QStringLiteral("下移行")},
        {QStringLiteral("Copy Lines Up"), QStringLiteral("向上复制行")},
        {QStringLiteral("Copy Lines Down"), QStringLiteral("向下复制行")},
        {QStringLiteral("Duplicate Selected Lines"), QStringLiteral("复制选中行")},
        {QStringLiteral("Join Lines"), QStringLiteral("合并行")},
        {QStringLiteral("Uppercase"), QStringLiteral("转换为大写")},
        {QStringLiteral("Lowercase"), QStringLiteral("转换为小写")},
        {QStringLiteral("Capitalize"), QStringLiteral("首字母大写")},
        {QStringLiteral("Comment"), QStringLiteral("注释")},
        {QStringLiteral("Uncomment"), QStringLiteral("取消注释")},
        {QStringLiteral("Go to Line..."), QStringLiteral("转到行...")},
        {QStringLiteral("Back"), QStringLiteral("后退")},
        {QStringLiteral("Forward"), QStringLiteral("前进")},
        {QStringLiteral("Last Edit Location"), QStringLiteral("上次编辑位置")},
        {QStringLiteral("Jump to Declaration"), QStringLiteral("跳转到声明")},
        {QStringLiteral("Jump to Definition"), QStringLiteral("跳转到定义")},
        {QStringLiteral("Jump to Next Outputmark"), QStringLiteral("跳转到下一个输出标记")},
        {QStringLiteral("Jump to Previous Outputmark"), QStringLiteral("跳转到上一个输出标记")},
        {QStringLiteral("Source Browse Mode"), QStringLiteral("源码浏览模式")},
        {QStringLiteral("Previous Visited Context"), QStringLiteral("上一个访问上下文")},
        {QStringLiteral("Next Visited Context"), QStringLiteral("下一个访问上下文")},
        {QStringLiteral("Previous Use"), QStringLiteral("上一个使用位置")},
        {QStringLiteral("Next Use"), QStringLiteral("下一个使用位置")},
        {QStringLiteral("Next Function"), QStringLiteral("下一个函数")},
        {QStringLiteral("Previous Function"), QStringLiteral("上一个函数")},
        {QStringLiteral("Outline"), QStringLiteral("大纲")},
        {QStringLiteral("Quick Open"), QStringLiteral("快速打开")},
        {QStringLiteral("Quick Open File"), QStringLiteral("快速打开文件")},
        {QStringLiteral("Quick Open Class"), QStringLiteral("快速打开类")},
        {QStringLiteral("Quick Open Function"), QStringLiteral("快速打开函数")},
        {QStringLiteral("Quick Open Already Open File"), QStringLiteral("快速打开已打开文件")},
        {QStringLiteral("Quick Open Documentation"), QStringLiteral("快速打开文档")},
        {QStringLiteral("Quick Open Actions"), QStringLiteral("快速打开动作")},
        {QStringLiteral("Embedded Quick Open"), QStringLiteral("嵌入式快速打开")},
        {QStringLiteral("Context Browser"), QStringLiteral("上下文浏览器")},
        {QStringLiteral("Document Declaration"), QStringLiteral("生成文档声明")},
        {QStringLiteral("Show Documentation"), QStringLiteral("显示文档")},
        {QStringLiteral("Reformat Source"), QStringLiteral("重新格式化源码")},
        {QStringLiteral("Reformat Line"), QStringLiteral("重新格式化当前行")},
        {QStringLiteral("Reformat Files..."), QStringLiteral("重新格式化文件...")},
        {QStringLiteral("Configure Launches..."), QStringLiteral("配置启动项...")},
        {QStringLiteral("Execute Launch"), QStringLiteral("运行启动项")},
        {QStringLiteral("Debug Launch"), QStringLiteral("调试启动项")},
        {QStringLiteral("Stop All Jobs"), QStringLiteral("停止所有任务")},
        {QStringLiteral("Stop Jobs"), QStringLiteral("停止任务")},
        {QStringLiteral("Continue"), QStringLiteral("继续")},
        {QStringLiteral("Pause"), QStringLiteral("暂停")},
        {QStringLiteral("Restart"), QStringLiteral("重新启动")},
        {QStringLiteral("Run to Cursor"), QStringLiteral("运行到光标处")},
        {QStringLiteral("Jump to Cursor"), QStringLiteral("跳转到光标处")},
        {QStringLiteral("Step Over"), QStringLiteral("单步跳过")},
        {QStringLiteral("Step Into"), QStringLiteral("单步进入")},
        {QStringLiteral("Step Out"), QStringLiteral("单步跳出")},
        {QStringLiteral("Toggle Breakpoint"), QStringLiteral("切换断点")},
        {QStringLiteral("Show Current Line"), QStringLiteral("显示当前行")},
        {QStringLiteral("Split View Top/Bottom"), QStringLiteral("上下拆分视图")},
        {QStringLiteral("Split View Left/Right"), QStringLiteral("左右拆分视图")},
        {QStringLiteral("Next Split View"), QStringLiteral("下一个拆分视图")},
        {QStringLiteral("Previous Split View"), QStringLiteral("上一个拆分视图")},
        {QStringLiteral("Next Window"), QStringLiteral("下一个窗口")},
        {QStringLiteral("Previous Window"), QStringLiteral("上一个窗口")},
        {QStringLiteral("Add Tool View..."), QStringLiteral("添加工具视图...")},
        {QStringLiteral("Show Left Dock"), QStringLiteral("显示左侧栏")},
        {QStringLiteral("Show Right Dock"), QStringLiteral("显示右侧栏")},
        {QStringLiteral("Show Bottom Dock"), QStringLiteral("显示底部栏")},
        {QStringLiteral("Hide All Docks"), QStringLiteral("隐藏所有侧栏")},
        {QStringLiteral("Focus Editor"), QStringLiteral("聚焦编辑器")},
        {QStringLiteral("Toggle Concentration Mode"), QStringLiteral("切换专注模式")},
        {QStringLiteral("Full Screen Mode"), QStringLiteral("全屏模式")},
        {QStringLiteral("Show Toolbar"), QStringLiteral("显示工具栏")},
        {QStringLiteral("Show Statusbar"), QStringLiteral("显示状态栏")},
        {QStringLiteral("Show Menubar"), QStringLiteral("显示菜单栏")},
        {QStringLiteral("Show Icon Border"), QStringLiteral("显示图标边栏")},
        {QStringLiteral("Show Line Numbers"), QStringLiteral("显示行号")},
        {QStringLiteral("Show Folding Markers"), QStringLiteral("显示折叠标记")},
        {QStringLiteral("Show Scrollbar Marks"), QStringLiteral("显示滚动条标记")},
        {QStringLiteral("Show Mini-Map"), QStringLiteral("显示迷你地图")},
        {QStringLiteral("Show Word Count"), QStringLiteral("显示字数统计")},
        {QStringLiteral("Dynamic Word Wrap"), QStringLiteral("动态自动换行")},
        {QStringLiteral("Increase Font Sizes"), QStringLiteral("增大字体")},
        {QStringLiteral("Decrease Font Sizes"), QStringLiteral("减小字体")},
        {QStringLiteral("Enlarge Font"), QStringLiteral("增大字体")},
        {QStringLiteral("Shrink Font"), QStringLiteral("减小字体")},
        {QStringLiteral("Fold Toplevel Nodes"), QStringLiteral("折叠顶层节点")},
        {QStringLiteral("Unfold Toplevel Nodes"), QStringLiteral("展开顶层节点")},
        {QStringLiteral("Fold Current Node"), QStringLiteral("折叠当前节点")},
        {QStringLiteral("Unfold Current Node"), QStringLiteral("展开当前节点")},
        {QStringLiteral("Set Bookmark"), QStringLiteral("设置书签")},
        {QStringLiteral("Clear Bookmarks"), QStringLiteral("清除书签")},
        {QStringLiteral("Previous Bookmark"), QStringLiteral("上一个书签")},
        {QStringLiteral("Next Bookmark"), QStringLiteral("下一个书签")},
        {QStringLiteral("Clear Current Line"), QStringLiteral("清除当前行书签")},
        {QStringLiteral("Configure Shortcuts..."), QStringLiteral("配置快捷键...")},
        {QStringLiteral("Configure Toolbars..."), QStringLiteral("配置工具栏...")},
        {QStringLiteral("Configure Notifications..."), QStringLiteral("配置通知...")},
        {QStringLiteral("Configure RRISE..."), QStringLiteral("配置 RRISE...")},
        {QStringLiteral("Configure KDevelop..."), QStringLiteral("配置 RRISE...")},
        {QStringLiteral("Loaded Plugins"), QStringLiteral("已加载插件")},
        {QStringLiteral("About RRISE"), QStringLiteral("关于 RRISE")},
        {QStringLiteral("About KDevelop"), QStringLiteral("关于 RRISE")},
        {QStringLiteral("About KDE"), QStringLiteral("关于 KDE")},
        {QStringLiteral("Report Bug..."), QStringLiteral("报告问题...")},
        {QStringLiteral("What's This?"), QStringLiteral("这是什么？")},
        {QStringLiteral("Donate"), QStringLiteral("捐助")},
        {QStringLiteral("Switch Application Language..."), QStringLiteral("切换应用程序语言...")},
        {QStringLiteral("Help Contents"), QStringLiteral("帮助手册")},
        {QStringLiteral("Back to Code"), QStringLiteral("返回代码")},
        {QStringLiteral("Current Document Directory"), QStringLiteral("当前文档目录")},
        {QStringLiteral("Locate Current Document"), QStringLiteral("定位当前文档")},
        {QStringLiteral("Commit Current Project..."), QStringLiteral("提交当前工程...")},
        {QStringLiteral("Remove from Build Set"), QStringLiteral("从构建集中移除")},
        {QStringLiteral("Build All Projects"), QStringLiteral("构建所有工程")},
        {QStringLiteral("Build Selection"), QStringLiteral("构建选中项")},
        {QStringLiteral("Install Selection"), QStringLiteral("安装选中项")},
        {QStringLiteral("Clean Selection"), QStringLiteral("清理选中项")},
        {QStringLiteral("Configure Selection"), QStringLiteral("配置选中项")},
        {QStringLiteral("Prune Selection"), QStringLiteral("裁剪选中项")},
        {QStringLiteral("Assign Shortcut..."), QStringLiteral("分配快捷键...")},
        {QStringLiteral("Remove Tool View"), QStringLiteral("移除工具视图")},
    };
    return translations;
}

const QHash<QString, QString>& actionNameTranslations()
{
    static const QHash<QString, QString> translations = {
        {QStringLiteral("file_new"), QStringLiteral("新建")},
        {QStringLiteral("file_open"), QStringLiteral("打开...")},
        {QStringLiteral("file_open_recent"), QStringLiteral("打开最近使用")},
        {QStringLiteral("file_save"), QStringLiteral("保存")},
        {QStringLiteral("file_save_as"), QStringLiteral("另存为...")},
        {QStringLiteral("file_save_all"), QStringLiteral("全部保存")},
        {QStringLiteral("file_close"), QStringLiteral("关闭")},
        {QStringLiteral("file_close_all"), QStringLiteral("全部关闭")},
        {QStringLiteral("file_closeother"), QStringLiteral("关闭其他")},
        {QStringLiteral("file_quit"), QStringLiteral("退出")},
        {QStringLiteral("edit_undo"), QStringLiteral("撤销")},
        {QStringLiteral("edit_redo"), QStringLiteral("重做")},
        {QStringLiteral("edit_cut"), QStringLiteral("剪切")},
        {QStringLiteral("edit_copy"), QStringLiteral("复制")},
        {QStringLiteral("edit_paste"), QStringLiteral("粘贴")},
        {QStringLiteral("edit_select_all"), QStringLiteral("全选")},
        {QStringLiteral("edit_deselect"), QStringLiteral("取消选择")},
        {QStringLiteral("edit_select_matching_bracket"), QStringLiteral("选择到匹配括号")},
        {QStringLiteral("edit_toggle_block_selection"), QStringLiteral("切换块选择")},
        {QStringLiteral("edit_insert_tab"), QStringLiteral("插入制表符")},
        {QStringLiteral("edit_indent"), QStringLiteral("增加缩进")},
        {QStringLiteral("edit_unindent"), QStringLiteral("减少缩进")},
        {QStringLiteral("edit_clean_indent"), QStringLiteral("清理缩进")},
        {QStringLiteral("edit_align"), QStringLiteral("对齐")},
        {QStringLiteral("edit_comment"), QStringLiteral("注释")},
        {QStringLiteral("edit_uncomment"), QStringLiteral("取消注释")},
        {QStringLiteral("edit_uppercase"), QStringLiteral("转换为大写")},
        {QStringLiteral("edit_lowercase"), QStringLiteral("转换为小写")},
        {QStringLiteral("edit_capitalize"), QStringLiteral("首字母大写")},
        {QStringLiteral("edit_join_lines"), QStringLiteral("合并行")},
        {QStringLiteral("edit_delete_line"), QStringLiteral("删除当前行")},
        {QStringLiteral("edit_delete_word_left"), QStringLiteral("删除左侧单词")},
        {QStringLiteral("edit_delete_word_right"), QStringLiteral("删除右侧单词")},
        {QStringLiteral("edit_move_line_up"), QStringLiteral("上移行")},
        {QStringLiteral("edit_move_line_down"), QStringLiteral("下移行")},
        {QStringLiteral("edit_copy_line_up"), QStringLiteral("向上复制行")},
        {QStringLiteral("edit_copy_line_down"), QStringLiteral("向下复制行")},
        {QStringLiteral("edit_find"), QStringLiteral("查找...")},
        {QStringLiteral("edit_find_next"), QStringLiteral("查找下一个")},
        {QStringLiteral("edit_find_prev"), QStringLiteral("查找上一个")},
        {QStringLiteral("edit_replace"), QStringLiteral("替换...")},
        {QStringLiteral("edit_grep"), QStringLiteral("在文件中查找/替换...")},
        {QStringLiteral("go_goto_line"), QStringLiteral("转到行...")},
        {QStringLiteral("to_matching_bracket"), QStringLiteral("转到匹配括号")},
        {QStringLiteral("select_matching_bracket"), QStringLiteral("选择到匹配括号")},
        {QStringLiteral("go_next_modified_line"), QStringLiteral("下一个已修改行")},
        {QStringLiteral("go_prev_modified_line"), QStringLiteral("上一个已修改行")},
        {QStringLiteral("bookmarks"), QStringLiteral("书签")},
        {QStringLiteral("set_bookmark"), QStringLiteral("设置书签")},
        {QStringLiteral("clear_bookmarks"), QStringLiteral("清除书签")},
        {QStringLiteral("next_bookmark"), QStringLiteral("下一个书签")},
        {QStringLiteral("prev_bookmark"), QStringLiteral("上一个书签")},
        {QStringLiteral("view_line_numbers"), QStringLiteral("显示行号")},
        {QStringLiteral("view_folding_markers"), QStringLiteral("显示折叠标记")},
        {QStringLiteral("view_icon_border"), QStringLiteral("显示图标边栏")},
        {QStringLiteral("view_scrollbar_marks"), QStringLiteral("显示滚动条标记")},
        {QStringLiteral("view_scrollbar_minimap"), QStringLiteral("显示迷你地图")},
        {QStringLiteral("view_scrollbar_preview"), QStringLiteral("显示滚动条预览")},
        {QStringLiteral("view_word_count"), QStringLiteral("显示字数统计")},
        {QStringLiteral("view_dynamic_word_wrap"), QStringLiteral("动态自动换行")},
        {QStringLiteral("view_word_wrap_marker"), QStringLiteral("显示自动换行标记")},
        {QStringLiteral("view_non_printable_spaces"), QStringLiteral("显示不可打印空白")},
        {QStringLiteral("view_indent_lines"), QStringLiteral("显示缩进线")},
        {QStringLiteral("view_toggle_write_lock"), QStringLiteral("切换只读锁定")},
        {QStringLiteral("view_full_screen"), QStringLiteral("全屏模式")},
        {QStringLiteral("tools_scripts"), QStringLiteral("脚本")},
        {QStringLiteral("tools_mode"), QStringLiteral("模式")},
        {QStringLiteral("tools_highlighting"), QStringLiteral("语法高亮")},
        {QStringLiteral("tools_indentation"), QStringLiteral("缩进")},
        {QStringLiteral("tools_encoding"), QStringLiteral("编码")},
        {QStringLiteral("tools_eol"), QStringLiteral("行尾格式")},
        {QStringLiteral("tools_apply_wordwrap"), QStringLiteral("应用自动换行")},
        {QStringLiteral("tools_cleanIndent"), QStringLiteral("清理缩进")},
        {QStringLiteral("tools_align"), QStringLiteral("对齐")},
        {QStringLiteral("tools_comment"), QStringLiteral("注释")},
        {QStringLiteral("tools_uncomment"), QStringLiteral("取消注释")},
        {QStringLiteral("tools_uppercase"), QStringLiteral("转换为大写")},
        {QStringLiteral("tools_lowercase"), QStringLiteral("转换为小写")},
        {QStringLiteral("tools_capitalize"), QStringLiteral("首字母大写")},
        {QStringLiteral("tools_join_lines"), QStringLiteral("合并行")},
        {QStringLiteral("tools_invoke_code_completion"), QStringLiteral("调用代码补全")},
        {QStringLiteral("tools_spelling"), QStringLiteral("拼写检查")},
        {QStringLiteral("folding_toplevel"), QStringLiteral("折叠顶层节点")},
        {QStringLiteral("unfolding_toplevel"), QStringLiteral("展开顶层节点")},
        {QStringLiteral("folding_current"), QStringLiteral("折叠当前节点")},
        {QStringLiteral("unfolding_current"), QStringLiteral("展开当前节点")},
        {QStringLiteral("options_show_toolbar"), QStringLiteral("显示工具栏")},
        {QStringLiteral("options_show_statusbar"), QStringLiteral("显示状态栏")},
        {QStringLiteral("options_configure_keybinding"), QStringLiteral("配置快捷键...")},
        {QStringLiteral("options_configure_toolbars"), QStringLiteral("配置工具栏...")},
        {QStringLiteral("settings_configure"), QStringLiteral("配置 RRISE...")},
        {QStringLiteral("configure_notifications"), QStringLiteral("配置通知...")},
        {QStringLiteral("loaded_plugins"), QStringLiteral("已加载插件")},
        {QStringLiteral("project_new"), QStringLiteral("从模板新建...")},
        {QStringLiteral("project_open"), QStringLiteral("打开/导入工程...")},
        {QStringLiteral("project_fetch"), QStringLiteral("获取工程...")},
        {QStringLiteral("project_open_recent"), QStringLiteral("打开最近工程")},
        {QStringLiteral("project_open_for_file"), QStringLiteral("打开当前文件所属工程")},
        {QStringLiteral("project_open_config"), QStringLiteral("打开配置...")},
        {QStringLiteral("project_close"), QStringLiteral("关闭工程")},
        {QStringLiteral("project_close_all"), QStringLiteral("关闭所有工程")},
        {QStringLiteral("commit_current_project"), QStringLiteral("提交当前工程...")},
        {QStringLiteral("run_default_target"), QStringLiteral("当前启动目标")},
        {QStringLiteral("configure_launches"), QStringLiteral("配置启动项...")},
        {QStringLiteral("run_execute"), QStringLiteral("运行启动项")},
        {QStringLiteral("run_debug"), QStringLiteral("调试启动项")},
        {QStringLiteral("run_stop_all"), QStringLiteral("停止所有任务")},
        {QStringLiteral("run_stop_menu"), QStringLiteral("停止任务")},
        {QStringLiteral("source_browse_mode"), QStringLiteral("源码浏览模式")},
        {QStringLiteral("prev_error"), QStringLiteral("跳转到上一个输出标记")},
        {QStringLiteral("next_error"), QStringLiteral("跳转到下一个输出标记")},
        {QStringLiteral("split_horizontal"), QStringLiteral("上下拆分视图")},
        {QStringLiteral("split_vertical"), QStringLiteral("左右拆分视图")},
        {QStringLiteral("view_next_split"), QStringLiteral("下一个拆分视图")},
        {QStringLiteral("view_previous_split"), QStringLiteral("上一个拆分视图")},
        {QStringLiteral("view_next_window"), QStringLiteral("下一个窗口")},
        {QStringLiteral("view_previous_window"), QStringLiteral("上一个窗口")},
        {QStringLiteral("add_toolview"), QStringLiteral("添加工具视图...")},
        {QStringLiteral("show_left_dock"), QStringLiteral("显示左侧栏")},
        {QStringLiteral("show_right_dock"), QStringLiteral("显示右侧栏")},
        {QStringLiteral("show_bottom_dock"), QStringLiteral("显示底部栏")},
        {QStringLiteral("hide_all_docks"), QStringLiteral("隐藏所有侧栏")},
        {QStringLiteral("focus_editor"), QStringLiteral("聚焦编辑器")},
        {QStringLiteral("toggle_concentration_mode"), QStringLiteral("切换专注模式")},
        {QStringLiteral("quick_open"), QStringLiteral("快速打开")},
        {QStringLiteral("quick_open_file"), QStringLiteral("快速打开文件")},
        {QStringLiteral("quick_open_class"), QStringLiteral("快速打开类")},
        {QStringLiteral("quick_open_function"), QStringLiteral("快速打开函数")},
        {QStringLiteral("quick_open_already_open"), QStringLiteral("快速打开已打开文件")},
        {QStringLiteral("quick_open_documentation"), QStringLiteral("快速打开文档")},
        {QStringLiteral("quick_open_actions"), QStringLiteral("快速打开动作")},
        {QStringLiteral("quick_open_jump_declaration"), QStringLiteral("跳转到声明")},
        {QStringLiteral("quick_open_jump_definition"), QStringLiteral("跳转到定义")},
        {QStringLiteral("quick_open_next_function"), QStringLiteral("下一个函数")},
        {QStringLiteral("quick_open_prev_function"), QStringLiteral("上一个函数")},
        {QStringLiteral("quick_open_outline"), QStringLiteral("大纲")},
        {QStringLiteral("edit_reformat_source"), QStringLiteral("重新格式化源码")},
        {QStringLiteral("edit_reformat_line"), QStringLiteral("重新格式化当前行")},
        {QStringLiteral("tools_astyle"), QStringLiteral("重新格式化文件...")},
        {QStringLiteral("document_declaration"), QStringLiteral("生成文档声明")},
        {QStringLiteral("showDocumentation"), QStringLiteral("显示文档")},
    };
    return translations;
}

void localizeMenu(QMenu* menu)
{
    if (!menu) {
        return;
    }

    for (QAction* action : menu->actions()) {
        if (!action || action->isSeparator()) {
            continue;
        }

        const auto nameIt = actionNameTranslations().constFind(action->objectName());
        if (nameIt != actionNameTranslations().constEnd()) {
            action->setText(*nameIt);
        } else {
            const QString key = normalizedMenuTitle(action->text());
            const auto textIt = actionTextTranslations().constFind(key);
            if (textIt != actionTextTranslations().constEnd()) {
                action->setText(*textIt);
            }
        }

        QMenu* const subMenu = action->menu();
        if (!subMenu) {
            continue;
        }

        const QString menuKey = normalizedMenuTitle(action->text());
        const auto titleIt = menuTitleTranslations().constFind(menuKey);
        if (titleIt != menuTitleTranslations().constEnd()) {
            action->setText(*titleIt);
            subMenu->setTitle(*titleIt);
        }
        localizeMenu(subMenu);
    }
}

}

void MainWindow::createGUI(KParts::Part* part)
{
    Sublime::MainWindow::setWindowTitleHandling(false);
    Sublime::MainWindow::createGUI(part);
    localizeTopLevelMenus();
    QTimer::singleShot(0, this, &MainWindow::localizeTopLevelMenus);
}

void MainWindow::localizeTopLevelMenus()
{
    for (QAction* action : menuBar()->actions()) {
        QMenu* const menu = action->menu();
        if (!menu) {
            continue;
        }

        const QString key = normalizedMenuTitle(action->text());
        const auto titleIt = menuTitleTranslations().constFind(key);
        if (titleIt != menuTitleTranslations().constEnd()) {
            action->setText(*titleIt);
            menu->setTitle(*titleIt);
        }

        if (!menu->property("_rrise_menu_localized").toBool()) {
            menu->setProperty("_rrise_menu_localized", true);
            connect(menu, &QMenu::aboutToShow, menu, [menu] {
                rriseApplyReadableDarkMenu(menu);
                localizeMenu(menu);
            });
        }
        rriseApplyReadableDarkMenu(menu);
        localizeMenu(menu);
    }
}

void MainWindow::applyRriseWindowThemeStyleSheet()
{
    if (m_rriseThemeStyleSheetApplying) {
        return;
    }

    m_rriseThemeStyleSheetApplying = true;
    const bool dark = rriseIsDarkPalette();
    rriseApplyReadableDarkTextPalette(this, dark);
    rriseApplyReadableDarkTextPalette(menuBar(), dark, rriseDarkMenuBarStyleSheet());
    rriseApplyReadableDarkTextPalette(statusBar(), dark);
    for (QAction* action : menuBar()->actions()) {
        rriseApplyReadableDarkMenu(action->menu());
    }
    for (auto* toolbar : findChildren<QToolBar*>()) {
        rriseApplyReadableDarkTextPalette(toolbar, dark, rriseDarkToolBarStyleSheet());
    }
    for (auto* dock : findChildren<QDockWidget*>()) {
        rriseApplyReadableDarkTextPalette(dock, dark);
    }
    for (auto* button : findChildren<QToolButton*>()) {
        rriseApplyReadableDarkTextPalette(button, dark, rriseDarkToolButtonStyleSheet());
        if (QMenu* menu = button->menu()) {
            rriseApplyReadableDarkMenu(menu);
        }
    }
    if (!styleSheet().isEmpty()) {
        setStyleSheet(QString{});
    }
    m_rriseThemeStyleSheetApplying = false;
}

void MainWindow::scheduleRriseWindowThemeStyleSheetUpdate()
{
    if (m_rriseThemeStyleSheetUpdatePending) {
        return;
    }

    m_rriseThemeStyleSheetUpdatePending = true;
    QTimer::singleShot(0, this, [this] {
        m_rriseThemeStyleSheetUpdatePending = false;
        applyRriseWindowThemeStyleSheet();
    });
}

void MainWindow::loadCornerSettings()
{
    const KConfigGroup cg = KSharedConfig::openConfig()->group(QStringLiteral("UiSettings"));
    const int bottomleft = cg.readEntry( "BottomLeftCornerOwner", 0 );
    const int bottomright = cg.readEntry( "BottomRightCornerOwner", 0 );
    qCDebug(SHELL) << "Bottom Left:" << bottomleft;
    qCDebug(SHELL) << "Bottom Right:" << bottomright;

    // 0 means vertical dock (left, right), 1 means horizontal dock( top, bottom )
    if( bottomleft == 0 )
        setCorner( Qt::BottomLeftCorner, Qt::LeftDockWidgetArea );
    else if( bottomleft == 1 )
        setCorner( Qt::BottomLeftCorner, Qt::BottomDockWidgetArea );

    if( bottomright == 0 )
        setCorner( Qt::BottomRightCorner, Qt::RightDockWidgetArea );
    else if( bottomright == 1 )
        setCorner( Qt::BottomRightCorner, Qt::BottomDockWidgetArea );
}

MainWindow::MainWindow( Sublime::Controller *parent, Qt::WindowFlags flags )
        : Sublime::MainWindow( parent, flags )
{
    QDBusConnection::sessionBus().registerObject( QStringLiteral("/kdevelop/MainWindow"),
        this, QDBusConnection::ExportScriptableSlots );

    setAcceptDrops( true );

    setObjectName( QStringLiteral("MainWindow") );
    d_ptr = new MainWindowPrivate(this);

    Q_D(MainWindow);

    setStandardToolBarMenuEnabled( true );
    d->setupActions();

    if( !ShellExtension::getInstance()->xmlFile().isEmpty() )
    {
        setXMLFile( ShellExtension::getInstance() ->xmlFile() );
    }

    menuBar()->setCornerWidget(new AreaDisplay(this), Qt::TopRightCorner);
    qApp->installEventFilter(this);
    scheduleRriseWindowThemeStyleSheetUpdate();
}

MainWindow::~ MainWindow()
{
    if (memberList().count() == 1) {
        // We're closing down...
        Core::self()->shutdown();
    }

    delete d_ptr;
}

KTextEditorIntegration::MainWindow *MainWindow::kateWrapper() const
{
    Q_D(const MainWindow);

    return d->kateWrapper();
}

void MainWindow::split(Qt::Orientation orientation)
{
    Q_D(MainWindow);

    d->split(orientation);
}

void MainWindow::loadUiPreferences()
{
    loadCornerSettings();
    updateAllTabColors();
    Sublime::MainWindow::loadUiPreferences();
}

void MainWindow::ensureVisible()
{
    if (isMinimized()) {
        if (isMaximized()) {
            showMaximized();
        } else {
            showNormal();
        }
    }

#if HAVE_X11
    if (KWindowSystem::isPlatformX11()) {
        KX11Extras::forceActiveWindow(winId());
        return;
    }
#endif
    activateWindow();
}

QAction* MainWindow::createCustomElement(QWidget* parent, int index, const QDomElement& element)
{
    QAction* before = nullptr;
    if (index > 0 && index < parent->actions().count())
        before = parent->actions().at(index);

    //KDevelop needs to ensure that separators defined as <Separator style="visible" />
    //are always shown in the menubar. For those, we create special disabled actions
    //instead of calling QMenuBar::addSeparator() because menubar separators are ignored
    if (element.tagName().compare(QLatin1String("separator"), Qt::CaseInsensitive) == 0
            && element.attribute(QStringLiteral("style")) == QLatin1String("visible")) {
        if ( auto* bar = qobject_cast<QMenuBar*>( parent ) ) {
            auto* separatorAction = new QAction(QStringLiteral("|"), this);
            bar->insertAction( before, separatorAction );
            separatorAction->setDisabled(true);
            return separatorAction;
        }
    }

    return KXMLGUIBuilder::createCustomElement(parent, index, element);
}

bool KDevelop::MainWindow::event( QEvent* ev )
{
    if (ev->type() == QEvent::PaletteChange || ev->type() == QEvent::ApplicationPaletteChange) {
        updateAllTabColors();
        scheduleRriseWindowThemeStyleSheetUpdate();
    } else if (ev->type() == QEvent::Show
               || ev->type() == QEvent::ChildAdded
               || ev->type() == QEvent::LayoutRequest
               || ev->type() == QEvent::PolishRequest) {
        scheduleRriseWindowThemeStyleSheetUpdate();
    }
    return Sublime::MainWindow::event(ev);
}

bool KDevelop::MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Show
        || event->type() == QEvent::Polish
        || event->type() == QEvent::PolishRequest
        || event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange) {
        if (auto* menu = qobject_cast<QMenu*>(watched)) {
            rriseApplyReadableDarkMenu(menu);
        }
    }

    return Sublime::MainWindow::eventFilter(watched, event);
}

void MainWindow::dragEnterEvent( QDragEnterEvent* ev )
{
    const QMimeData* mimeData = ev->mimeData();
    if (mimeData->hasUrls()) {
        ev->acceptProposedAction();
    } else if (mimeData->hasText()) {
        // also take text which contains a URL
        const QUrl url = QUrl::fromUserInput(mimeData->text());
        if (url.isValid()) {
            ev->acceptProposedAction();
        }
    }
}

void MainWindow::dropEvent( QDropEvent* ev )
{
    Sublime::View* dropToView = viewForPosition(mapToGlobal(ev->position().toPoint()));
    if(dropToView)
        activateView(dropToView);

    QList<QUrl> urls;

    const QMimeData* mimeData = ev->mimeData();
    if (mimeData->hasUrls()) {
        urls = mimeData->urls();
    } else if (mimeData->hasText()) {
        const QUrl url = QUrl::fromUserInput(mimeData->text());
        if (url.isValid()) {
            urls << url;
        }
    }

    bool eventUsed = false;
    if (urls.size() == 1) {
        eventUsed = Core::self()->projectControllerInternal()->fetchProjectFromUrl(urls.at(0), ProjectController::NoFetchFlags);
    }

    if (!eventUsed) {
        for (const auto& url : std::as_const(urls)) {
            Core::self()->documentController()->openDocument(url);
        }
    }

    ev->acceptProposedAction();
}

void MainWindow::loadSettings()
{
    Q_D(const MainWindow);

    if (!d->isGuiSetUp()) {
        // UiController() invokes this->loadSettings() before UiController::initialize() invokes this->initialize().
        // this->initialize() invokes KXmlGuiWindow::setupGUI(), which resizes the window to its default size.
        // Sublime::MainWindow::loadSettings() calls KXmlGuiWindow::applyMainWindowSettings(), which applies the size
        // stored in the main window config *once*. If the size stored in config is applied before the default resizing,
        // the window always ends up having the default size. Prevent this by returning early if the GUI is not set up
        // (not initialized) yet. The settings are still always loaded when UiController::loadAllAreas() is invoked
        // after this->initialize().
        return;
    }

    qCDebug(SHELL) << "Loading Settings";

    Sublime::MainWindow::loadSettings();
    // Sublime::MainWindow::loadSettings() invokes QMainWindow::restoreState(), which restores corner
    // settings. Override the corner settings to make sure they match KDevelop's custom corner options.
    loadCornerSettings();
}

void MainWindow::configureShortcuts()
{
    ///Workaround for a problem with the actions: Always start the shortcut-configuration in the first mainwindow, then propagate the updated
    ///settings into the other windows


// We need to bring up the shortcut dialog ourself instead of
//      Core::self()->uiControllerInternal()->mainWindows()[0]->guiFactory()->configureShortcuts();
// so we can connect to the saved() signal to propagate changes in the editor shortcuts

   KShortcutsDialog dlg(KShortcutsEditor::AllActions, KShortcutsEditor::LetterShortcutsAllowed, this);

    const auto firstMainWindowClientsBefore = Core::self()->uiControllerInternal()->mainWindows()[0]->guiFactory()->clients();
    for (KXMLGUIClient* client : firstMainWindowClientsBefore) {
        if(client && !client->xmlFile().isEmpty())
            dlg.addCollection( client->actionCollection() );
    }

    connect(&dlg, &KShortcutsDialog::saved, this, &MainWindow::shortcutsChanged);
    dlg.configure(true);

    QMap<QString, QKeySequence> shortcuts;
    // querying again just in case something changed behind our back
    const auto firstMainWindowClientsAfter = Core::self()->uiControllerInternal()->mainWindows()[0]->guiFactory()->clients();
    for (KXMLGUIClient* client : firstMainWindowClientsAfter) {
        const auto actions = client->actionCollection()->actions();
        for (QAction* action : actions) {
            if(!action->objectName().isEmpty()) {
                shortcuts[action->objectName()] = action->shortcut();
            }
        }
    }

    for(int a = 1; a < Core::self()->uiControllerInternal()->mainWindows().size(); ++a) {
        const auto clients = Core::self()->uiControllerInternal()->mainWindows()[a]->guiFactory()->clients();
        for (KXMLGUIClient* client : clients) {
            const auto actions = client->actionCollection()->actions();
            for (QAction* action : actions) {
                qCDebug(SHELL) << "transferring setting shortcut for" << action->objectName();
                const auto shortcutIt = shortcuts.constFind(action->objectName());
                if (shortcutIt != shortcuts.constEnd()) {
                    action->setShortcut(*shortcutIt);
                }
            }
        }
    }

}

void MainWindow::shortcutsChanged()
{
    KTextEditor::View *activeClient = Core::self()->documentController()->activeTextDocumentView();
    if(!activeClient)
        return;

    const auto documents = Core::self()->documentController()->openDocuments();
    for (IDocument* doc : documents) {
        KTextEditor::Document *textDocument = doc->textDocument();
        if (textDocument) {
            const auto views = textDocument->views();
            for (KTextEditor::View* client : views) {
                if (client != activeClient) {
                    client->reloadXML();
                }
            }
        }
    }
}


void MainWindow::initialize()
{
    Q_D(MainWindow);

    KStandardAction::keyBindings(this, SLOT(configureShortcuts()), actionCollection());

    // Do not pass the Save option to setupGUI(), because main window settings are loaded from and saved to
    // different config groups depending on the current area. So KMainWindow::autoSaveGroup() would have to
    // be changed accordingly each time another area is switched to. Also Sublime::MainWindow::saveSettings()
    // and Sublime::MainWindow::loadSettings() do more than just call saveMainWindowSettings() and
    // applyMainWindowSettings() respectively. applyMainWindowSettings() is virtual and can be overridden, but
    // there is no way to customize behavior of the non-virtual function KMainWindow::saveMainWindowSettings().
    // Auto-saving also requires recovering from a possible last auto-save while in Concentration Mode - perhaps
    // force-exit Concentration Mode even if it is off at the end of Sublime::MainWindow::loadSettings().
    // On KDevelop exit, UiController::cleanup() calls Sublime::MainWindow::saveSettings() for each main window.
    // Therefore, auto-saving main window settings is useful only in case KDevelop crashes. Auto-saving correctly
    // might be possible, but perhaps not worth the likely significant implementation complexity increase.
    setupGUI(QSize{870, 650}, ToolBar);
    connect(guiFactory(), &KXMLGUIFactory::clientAdded, this, [this] {
        QTimer::singleShot(0, this, &MainWindow::localizeTopLevelMenus);
    });
    createGUI(nullptr);

    Core::self()->partController()->addManagedTopLevelWidget(this);
    qCDebug(SHELL) << "Adding plugin-added connection";

    connect( Core::self()->pluginController(), &IPluginController::pluginLoaded,
             d, &MainWindowPrivate::addPlugin);
    connect( Core::self()->pluginController(), &IPluginController::pluginUnloaded,
             d, &MainWindowPrivate::removePlugin);
    connect( Core::self()->partController(), &IPartController::activePartChanged,
        d, &MainWindowPrivate::activePartChanged);
    connect( this, &MainWindow::activeViewChanged,
        d, &MainWindowPrivate::changeActiveView);
    connect(Core::self()->sourceFormatterControllerInternal(), &SourceFormatterController::hasFormattersChanged,
             d, &MainWindowPrivate::updateSourceFormatterGuiClient);

    const auto plugins = Core::self()->pluginController()->loadedPlugins();
    for (IPlugin* plugin : plugins) {
        d->addPlugin(plugin);
    }

    guiFactory()->addClient(Core::self()->sessionController());
    if (Core::self()->sourceFormatterControllerInternal()->hasFormatters()) {
        guiFactory()->addClient(Core::self()->sourceFormatterControllerInternal());
    }

    // Needed to re-plug the actions from the sessioncontroller as xmlguiclients don't
    // seem to remember which actions where plugged in.
    Core::self()->sessionController()->updateXmlGuiActionList();

    d->setupGui();

    qRegisterMetaType<QPointer<KTextEditor::Document>>();

    //Queued so we process it with some delay, to make sure the rest of the UI has already adapted
    connect(Core::self()->documentController(), &IDocumentController::documentActivated,
            // Use a queued connection, because otherwise the view is not yet fully set up
            // but wrap the document in a smart pointer to guard against crashes when it
            // gets deleted in the meantime
            this, [this](IDocument *doc) {
                const auto textDocument = QPointer<KTextEditor::Document>(doc->textDocument());
                QMetaObject::invokeMethod(this, "documentActivated", Qt::QueuedConnection,
                                          Q_ARG(QPointer<KTextEditor::Document>, textDocument));
            });

    connect(Core::self()->documentController(), &IDocumentController::documentClosed, this, &MainWindow::updateCaption, Qt::QueuedConnection);
    connect(Core::self()->documentController(), &IDocumentController::documentUrlChanged, this, &MainWindow::updateCaption, Qt::QueuedConnection);
    connect(Core::self()->documentController(), &IDocumentController::documentStateChanged, this, &MainWindow::updateCaption, Qt::QueuedConnection);
    connect(Core::self()->sessionController()->activeSession(), &ISession::sessionUpdated, this, &MainWindow::updateCaption);
    // if currently viewed document is part of project, trigger update of full path to project prefixed one
    connect(Core::self()->projectController(), &ProjectController::projectOpened, this, &MainWindow::updateCaption, Qt::QueuedConnection);

    connect(Core::self()->documentController(), &IDocumentController::documentOpened, this, &MainWindow::updateTabColor);
    connect(Core::self()->documentController(), &IDocumentController::documentUrlChanged, this, &MainWindow::updateTabColor);
    connect(this, &Sublime::MainWindow::viewAdded, this, &MainWindow::updateAllTabColors);
    connect(Core::self()->projectController(), &ProjectController::projectOpened, this, &MainWindow::updateAllTabColors, Qt::QueuedConnection);

    updateCaption();
}

void MainWindow::cleanup()
{
}

bool MainWindow::queryClose()
{
    if (!Core::self()->documentControllerInternal()->saveAllDocumentsForWindow(
            this, IDocumentController::SaveSelectionMode::LetUserSelect)) {
        return false;
    }

    return Sublime::MainWindow::queryClose();
}

void MainWindow::documentActivated(const QPointer<KTextEditor::Document>& textDocument)
{
    Q_D(MainWindow);

    updateCaption();

    // update active document connection
    disconnect(d->activeDocumentReadWriteConnection);
    if (textDocument) {
        d->activeDocumentReadWriteConnection = connect(textDocument, &KTextEditor::Document::readWriteChanged,
                                                    this, &MainWindow::updateCaption);
    }
}

void MainWindow::updateCaption()
{
    QString title;
    QString localFilePath;
    bool isDocumentModified = false;

    if(area()->activeView())
    {
        Sublime::Document* doc = area()->activeView()->document();
        auto* urlDoc = qobject_cast<Sublime::UrlDocument*>(doc);
        if(urlDoc)
        {
            if (urlDoc->url().isLocalFile()) {
                localFilePath = urlDoc->url().toLocalFile();
            }
            title += Core::self()->projectController()->prettyFileName(urlDoc->url(), KDevelop::IProjectController::FormatPlain);
        }
        else
            title += doc->title();

        auto iDoc = qobject_cast<IDocument*>(doc);
        if (iDoc && iDoc->textDocument() && !iDoc->textDocument()->isReadWrite()) {
            title += i18n(" (read only)");
        }

        title += QLatin1String(" [*]"); // [*] is placeholder for modified state, cmp. QWidget::windowModified

        isDocumentModified = iDoc && (iDoc->state() != IDocument::Clean);
    }

    const auto activeSession = Core::self()->sessionController()->activeSession();
    const QString sessionTitle = activeSession ? activeSession->description() : QString();
    if (!sessionTitle.isEmpty()) {
        if (title.isEmpty()) {
            title = sessionTitle;
        } else {
            title = sessionTitle + QLatin1String(" - [ ") + title + QLatin1Char(']');
        }
    }

    // Workaround for a bug observed on macOS with Qt 5.9.8 (TODO: test with newer Qt, report bug):
    // Ensure to call setCaption() (thus implicitly setWindowTitle()) before
    // setWindowModified() & setWindowFilePath().
    // Otherwise, if the state will change "modified" from true to false as well change the title string,
    // calling setWindowTitle() last results in the "modified" indicator==asterisk becoming part of the
    // displayed window title somehow.
    // Other platforms so far not known to be affected, any order of calls seems fine.
    setCaption(title);
    setWindowModified(isDocumentModified);
    setWindowFilePath(localFilePath);
}

void MainWindow::updateAllTabColors()
{
    auto documentController = Core::self()->documentController();
    if (!documentController)
        return;

    const auto defaultColor = ::defaultColor(palette());
    if (UiConfig::colorizeByProject()) {
        QHash<const Sublime::View*, QColor> viewColors;
        const auto containers = this->containers();
        for (auto* container : containers) {
            const auto views = container->views();
            viewColors.reserve(views.size());
            viewColors.clear();
            for (auto view : views) {
                const auto urlDoc = qobject_cast<Sublime::UrlDocument*>(view->document());
                if (urlDoc) {
                    viewColors[view] = colorForDocument(urlDoc->url(), palette(), defaultColor);
                }
            }
            container->setTabColors(viewColors);
        }
    } else {
        const auto containers = this->containers();
        for (auto* container : containers) {
            container->resetTabColors(defaultColor);
        }
    }
}

void MainWindow::updateTabColor(IDocument* doc)
{
    if (!UiConfig::self()->colorizeByProject())
        return;

    const auto color = colorForDocument(doc->url(), palette(), defaultColor(palette()));
    const auto containers = this->containers();
    for (auto* container : containers) {
        const auto views = container->views();
        for (auto* view : views) {
            const auto urlDoc = qobject_cast<Sublime::UrlDocument*>(view->document());
            if (urlDoc && urlDoc->url() == doc->url()) {
                container->setTabColor(view, color);
            }
        }
    }
}

void MainWindow::registerStatus(QObject* status)
{
    Q_D(MainWindow);

    d->registerStatus(status);
}

void MainWindow::initializeStatusBar()
{
    Q_D(MainWindow);

    d->setupStatusBar();
}

void MainWindow::showErrorMessage(const QString& message, int timeout)
{
    Q_D(MainWindow);

    d->showErrorMessage(message, timeout);
}

void MainWindow::tabContextMenuRequested(Sublime::View* view, QMenu* menu)
{
    Q_D(MainWindow);

    Sublime::MainWindow::tabContextMenuRequested(view, menu);
    d->tabContextMenuRequested(view, menu);
}

void MainWindow::tabToolTipRequested(Sublime::View* view, Sublime::Container* container, int tab)
{
    Q_D(MainWindow);

    d->tabToolTipRequested(view, container, tab);
}

void MainWindow::dockBarContextMenuRequested(Qt::DockWidgetArea area, const QPoint& position)
{
    Q_D(MainWindow);

    d->dockBarContextMenuRequested(area, position);
}

void MainWindow::newTabRequested()
{
    Q_D(MainWindow);

    Sublime::MainWindow::newTabRequested();

    d->fileNew();
}

void MainWindow::saveNewToolbarConfig()
{
    // Applying a toolbar config removes actions added via KXMLGUIClient::plugActionList().
    Sublime::MainWindow::saveNewToolbarConfig();
    // So plug the available_sessions actions again as the documentation for KEditToolBar recommends.
    Core::self()->sessionController()->updateXmlGuiActionList();
}

#include "moc_mainwindow.cpp"
