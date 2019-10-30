/***************************************************************************************
* Original Author:      Gabriele Giuseppini
* Created:              2019-10-23
* Copyright:            Gabriele Giuseppini  (https://github.com/GabrieleGiuseppini)
***************************************************************************************/
#include "TextLayer.h"

#include <GameCore/GameWallClock.h>

#include <cassert>
#include <iomanip>
#include <sstream>

using namespace std::literals::chrono_literals;

TextLayer::TextLayer(
	std::shared_ptr<Render::TextRenderContext> textRenderContext,
    bool isStatusTextEnabled,
    bool isExtendedStatusTextEnabled)
    : mTextRenderContext(std::move(textRenderContext))
	// StatusText state
	, mIsStatusTextEnabled(isStatusTextEnabled)
	, mIsExtendedStatusTextEnabled(isExtendedStatusTextEnabled)
    , mStatusTextLines()
	, mAreStatusTextLinePositionsDirty(false)
	// Ephemeral text
	, mEphemeralTextLines()
{
}

void TextLayer::SetStatusTextEnabled(bool isEnabled)
{
	mIsStatusTextEnabled = isEnabled;

	// Positions need to be recalculated
	mAreStatusTextLinePositionsDirty = true;
}

void TextLayer::SetExtendedStatusTextEnabled(bool isEnabled)
{
	mIsExtendedStatusTextEnabled = isEnabled;

	// Positions need to be recalculated
	mAreStatusTextLinePositionsDirty = true;
}

void TextLayer::SetStatusTexts(
    float immediateFps,
    float averageFps,
    std::chrono::duration<float> elapsedGameSeconds,
    bool isPaused,
    float zoom,
    vec2f const & camera,
    float totalUpdateToRenderDurationRatio,
    float lastUpdateToRenderDurationRatio,
    Render::RenderStatistics const & renderStatistics)
{
    int elapsedSecondsGameInt = static_cast<int>(roundf(elapsedGameSeconds.count()));
    int minutesGame = elapsedSecondsGameInt / 60;
    int secondsGame = elapsedSecondsGameInt - (minutesGame * 60);

    //
    // Build text
    //

    if (mIsStatusTextEnabled)
    {
        std::ostringstream ss;

        ss.fill('0');

        ss << std::fixed << std::setprecision(2)
            << "FPS:" << averageFps << " (" << immediateFps << ")"
            << " " << std::setw(2) << minutesGame << ":" << std::setw(2) << secondsGame;

        if (isPaused)
            ss << " (PAUSED)";

		mStatusTextLines[0].Text = ss.str();
		mStatusTextLines[0].IsTextDirty = true;
    }

    if (mIsExtendedStatusTextEnabled)
    {
        std::ostringstream ss;

        ss.fill('0');

        ss << std::fixed << std::setprecision(2)
            << "U/R:" << (100.0f * totalUpdateToRenderDurationRatio) << "% (" << (100.0f * lastUpdateToRenderDurationRatio) << "%)"
            << " ZOOM:" << zoom
            << " CAM:" << camera.x << ", " << camera.y;

		mStatusTextLines[1].Text = ss.str();
		mStatusTextLines[1].IsTextDirty = true;

        ss.str("");

        ss
            << "PNT:" << renderStatistics.LastRenderedShipPoints
            << " RPS:" << renderStatistics.LastRenderedShipRopes
            << " SPR:" << renderStatistics.LastRenderedShipSprings
            << " TRI:" << renderStatistics.LastRenderedShipTriangles
            << " PLN:" << renderStatistics.LastRenderedShipPlanes
            << " GENTEX:" << renderStatistics.LastRenderedShipGenericTextures
            << " FLM:" << renderStatistics.LastRenderedShipFlames;

		mStatusTextLines[2].Text = ss.str();
		mStatusTextLines[2].IsTextDirty = true;
    }
}

void TextLayer::AddEphemeralTextLine(
	std::string const & text,
	std::chrono::duration<float> lifetime)
{
	// Create text
	auto handle = mTextRenderContext->AddTextLine(
		text,
		TextPositionType::TopRight,
		vec2f::zero(), // for now
		0.0f, // for now
		FontType::GameText);

	// Store ephemeral line
	mEphemeralTextLines.emplace_back(
		handle,
		lifetime);
}

void TextLayer::Update(float now)
{
	//
	// Status text
	//

	{
		int ordinal = 0;

		UpdateStatusTextLine(mStatusTextLines[0], mIsStatusTextEnabled, mAreStatusTextLinePositionsDirty, ordinal);

		for (size_t i = 1; i <= 2; ++i)
		{
			UpdateStatusTextLine(mStatusTextLines[i], mIsExtendedStatusTextEnabled, mAreStatusTextLinePositionsDirty, ordinal);
		}

		mAreStatusTextLinePositionsDirty = false;
	}


	//
	// Ephemeral lines
	//

	// 1) Trim first lines if we've got too many
	while (mEphemeralTextLines.size() > 8)
	{
		assert(mEphemeralTextLines.front().Handle != NoneRenderedTextHandle);
		mTextRenderContext->ClearTextLine(mEphemeralTextLines.front().Handle);
		mEphemeralTextLines.pop_front();
	}

	// 2) Update state of remaining ones
	vec2f screenOffset; // Cumulative vertical offset
	float const lineHeight = static_cast<float>(mTextRenderContext->GetLineScreenHeight(FontType::GameText));
	for (auto it = mEphemeralTextLines.begin(); it != mEphemeralTextLines.end(); )
	{
		bool doDeleteLine = false;

		switch (it->State)
		{
			case EphemeralTextLine::StateType::Initial:
			{
				// Initialize fade-in
				it->State = EphemeralTextLine::StateType::FadingIn;
				it->CurrentStateStartTimestamp = now;

				[[fallthrough]];
			}

			case EphemeralTextLine::StateType::FadingIn:
			{
				auto const progress = GameWallClock::Progress(now, it->CurrentStateStartTimestamp, 500ms);

				// Update text
				mTextRenderContext->UpdateTextLine(it->Handle, screenOffset, std::min(1.0f, progress));

				// See if time to transition
				if (progress >= 1.0f)
				{
					it->State = EphemeralTextLine::StateType::Displaying;
					it->CurrentStateStartTimestamp = now;
				}

				// Update offset of next line
				screenOffset.y += lineHeight;

				break;
			}

			case EphemeralTextLine::StateType::Displaying:
			{
				auto const progress = GameWallClock::Progress(now, it->CurrentStateStartTimestamp, it->Lifetime);

				// Update text
				mTextRenderContext->UpdateTextLine(it->Handle, screenOffset, 1.0f);

				// See if time to transition
				if (progress >= 1.0f)
				{
					it->State = EphemeralTextLine::StateType::FadingOut;
					it->CurrentStateStartTimestamp = now;
				}

				// Update offset of next line
				screenOffset.y += lineHeight;

				break;
			}

			case EphemeralTextLine::StateType::FadingOut:
			{
				auto const progress = GameWallClock::Progress(now, it->CurrentStateStartTimestamp, 500ms);

				// Update text
				mTextRenderContext->UpdateTextLine(it->Handle, screenOffset, 1.0f - std::min(1.0f, progress));

				// See if time to transition
				if (progress >= 1.0f)
				{
					it->State = EphemeralTextLine::StateType::Disappearing;
					it->CurrentStateStartTimestamp = now;
				}

				// Update offset of next line
				screenOffset.y += lineHeight;

				break;
			}

			case EphemeralTextLine::StateType::Disappearing:
			{
				auto const progress = GameWallClock::Progress(now, it->CurrentStateStartTimestamp, 500ms);

				// See if time to turn off
				if (progress >= 1.0f)
				{
					doDeleteLine = true;
				}

				// Update offset of next line
				screenOffset.y += lineHeight * (1.0f - std::min(1.0f, progress));

				break;
			}
		}

		// Advance
		if (doDeleteLine)
		{
			it = mEphemeralTextLines.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void TextLayer::UpdateStatusTextLine(
	StatusTextLine & line,
	bool isEnabled,
	bool arePositionsDirty,
	int & ordinal)
{
	// Check whether we need to flip the state of the status text
	if (isEnabled)
	{
		//
		// This line is enabled
		//

		vec2f offset(
			0.0f,
			static_cast<float>(ordinal++) * mTextRenderContext->GetLineScreenHeight(FontType::StatusText));

		if (NoneRenderedTextHandle == line.Handle)
		{
			// Create status text
			line.Handle = mTextRenderContext->AddTextLine(
				line.Text,
				TextPositionType::TopLeft,
				offset,
				1.0f,
				FontType::StatusText);

			line.IsTextDirty = false;
		}
		else if (line.IsTextDirty || arePositionsDirty)
		{
			// Update status text
			mTextRenderContext->UpdateTextLine(
				line.Handle,
				line.Text,
				offset);

			line.IsTextDirty = false;
		}		
	}
	else 
	{
		//
		// This line is not enabled
		//

		if (NoneRenderedTextHandle != line.Handle)
		{
			// Turn off line altogether
			mTextRenderContext->ClearTextLine(line.Handle);
			line.Handle = NoneRenderedTextHandle;

			// Initialize text
			line.Text = "";
			line.IsTextDirty = false;
		}
	}
}