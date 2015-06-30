// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/cmd/set_palette.h"
#include "app/commands/command.h"
#include "app/console.h"
#include "app/context.h"
#include "app/context_access.h"
#include "app/modules/palettes.h"
#include "app/transaction.h"
#include "app/ui/color_bar.h"
#include "app/ui_context.h"
#include "base/unique_ptr.h"
#include "base/unique_ptr.h"
#include "doc/palette.h"
#include "doc/sprite.h"
#include "render/quantization.h"
#include "ui/manager.h"

#include "generated_palette_from_sprite.h"

namespace app {

class ColorQuantizationCommand : public Command {
public:
  ColorQuantizationCommand();
  Command* clone() const override { return new ColorQuantizationCommand(*this); }

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

ColorQuantizationCommand::ColorQuantizationCommand()
  : Command("ColorQuantization",
            "Color Quantization",
            CmdRecordableFlag)
{
}

bool ColorQuantizationCommand::onEnabled(Context* context)
{
  ContextWriter writer(context);
  Sprite* sprite(writer.sprite());
  return (sprite && sprite->pixelFormat() == IMAGE_RGB);
}

void ColorQuantizationCommand::onExecute(Context* context)
{
  try {
    app::gen::PaletteFromSprite window;
    PalettePicks entries;

    Sprite* sprite;
    frame_t frame;
    Palette* curPalette;
    {
      ContextReader reader(context);
      Site site = context->activeSite();
      sprite = site.sprite();
      frame = site.frame();
      curPalette = sprite->palette(frame);

      window.newPalette()->setSelected(true);
      window.ncolors()->setText("256");

      ColorBar::instance()->getPaletteView()->getSelectedEntries(entries);
      if (entries.picks() > 1) {
        window.currentRange()->setTextf(
          "%s, %d color(s)",
          window.currentRange()->getText().c_str(),
          entries.picks());
      }
      else
        window.currentRange()->setEnabled(false);

      window.currentPalette()->setTextf(
        "%s, %d color(s)",
        window.currentPalette()->getText().c_str(),
        curPalette->size());
    }

    window.openWindowInForeground();
    if (window.getKiller() != window.ok())
      return;

    bool createPal = false;
    if (window.newPalette()->isSelected()) {
      int n = window.ncolors()->getTextInt();
      n = MAX(1, n);
      entries = PalettePicks(n);
      entries.all();
      createPal = true;
    }
    else if (window.currentPalette()->isSelected()) {
      entries.all();
    }
    if (entries.picks() == 0)
      return;

    Palette tmpPalette(frame, entries.picks());
    render::create_palette_from_rgb(sprite, 0, sprite->lastFrame(), &tmpPalette);

    base::UniquePtr<Palette> newPalette(
      new Palette(createPal ? tmpPalette:
                              *get_current_palette()));

    if (createPal) {
      entries = PalettePicks(newPalette->size());
      entries.all();
    }

    int i = 0, j = 0;
    for (bool state : entries) {
      if (state)
        newPalette->setEntry(i, tmpPalette.getEntry(j++));
      ++i;
    }

    if (*curPalette != *newPalette) {
      ContextWriter writer(UIContext::instance(), 500);
      Transaction transaction(writer.context(), "Color Quantization", ModifyDocument);
      transaction.execute(new cmd::SetPalette(sprite, frame, newPalette.get()));
      transaction.commit();

      set_current_palette(newPalette.get(), false);
      ui::Manager::getDefault()->invalidate();
    }
  }
  catch (base::Exception& e) {
    Console::showException(e);
  }
}

Command* CommandFactory::createColorQuantizationCommand()
{
  return new ColorQuantizationCommand;
}

} // namespace app
