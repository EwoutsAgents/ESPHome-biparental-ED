#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.home() / ".platformio/packages/framework-espidf/components/openthread/openthread/src/core"

PATCHES = [
    (
        ROOT / "thread/mle.hpp",
        "    Error SearchForBetterParent(void);\n",
        """    Error SearchForBetterParent(void);\n\n    /**\n     * Starts targeted attach flow to a selected parent (by extended address).\n     *\n     * This uses internal selected-parent attach mode and is intended for controlled\n     * parent switching flows that need a specific parent candidate.\n     *\n     * @param[in] aExtAddress  Extended address of the selected parent candidate.\n     *\n     * @retval kErrorNone          Successfully started selected-parent attach flow.\n     * @retval kErrorInvalidState  Thread is disabled or not attached as child.\n     * @retval kErrorBusy          An attach process is already in progress.\n     */\n    Error AttachToSelectedParent(const Mac::ExtAddress &aExtAddress);\n""",
    ),
    (
        ROOT / "thread/mle.cpp",
        """Error Mle::SearchForBetterParent(void)
{
    Error error = kErrorNone;

    VerifyOrExit(IsChild(), error = kErrorInvalidState);
    mAttacher.Attach(kBetterParent);

exit:
    return error;
}
""",
        """Error Mle::SearchForBetterParent(void)
{
    Error error = kErrorNone;

    VerifyOrExit(IsChild(), error = kErrorInvalidState);
    mAttacher.Attach(kBetterParent);

exit:
    return error;
}

Error Mle::AttachToSelectedParent(const Mac::ExtAddress &aExtAddress)
{
    Error error = kErrorNone;

    VerifyOrExit(IsChild(), error = kErrorInvalidState);
    VerifyOrExit(!IsAttaching(), error = kErrorBusy);

    mAttacher.Attach(kSelectedParent);
    mAttacher.GetParentCandidate().SetExtAddress(aExtAddress);

exit:
    return error;
}
""",
    ),
    (
        ROOT / "thread/mle.cpp",
        """#if OPENTHREAD_FTD && OPENTHREAD_CONFIG_PARENT_SEARCH_ENABLE
    if (aType == kToSelectedRouter)
    {
        TxMessage *messageToCurParent = static_cast<TxMessage *>(message->Clone());

        VerifyOrExit(messageToCurParent != nullptr, error = kErrorNoBufs);

        destination.SetToLinkLocalAddress(Get<Mle>().mParent.GetExtAddress());
        error = messageToCurParent->SendTo(destination);

        if (error != kErrorNone)
        {
            messageToCurParent->Free();
            ExitNow();
        }

        Log(kMessageSend, kTypeParentRequestToRouters, destination);

        destination.SetToLinkLocalAddress(Get<Mle>().mParentSearch.GetSelectedParent().GetExtAddress());
    }
    else
#endif
    {
        destination.SetToLinkLocalAllRoutersMulticast();
    }
""",
        """    if (aType == kToSelectedRouter)
    {
#if OPENTHREAD_FTD && OPENTHREAD_CONFIG_PARENT_SEARCH_ENABLE
        TxMessage *messageToCurParent = static_cast<TxMessage *>(message->Clone());

        VerifyOrExit(messageToCurParent != nullptr, error = kErrorNoBufs);

        destination.SetToLinkLocalAddress(Get<Mle>().mParent.GetExtAddress());
        error = messageToCurParent->SendTo(destination);

        if (error != kErrorNone)
        {
            messageToCurParent->Free();
            ExitNow();
        }

        Log(kMessageSend, kTypeParentRequestToRouters, destination);

        destination.SetToLinkLocalAddress(Get<Mle>().mParentSearch.GetSelectedParent().GetExtAddress());
#else
        destination.SetToLinkLocalAddress(mParentCandidate.GetExtAddress());
#endif
    }
    else
    {
        destination.SetToLinkLocalAllRoutersMulticast();
    }
""",
    ),
    (
        ROOT / "thread/mle.cpp",
        """    SuccessOrExit(error = aRxInfo.mMessage.ReadAndMatchResponseTlvWith(mParentRequestChallenge));
""",
        """    error = aRxInfo.mMessage.ReadAndMatchResponseTlvWith(mParentRequestChallenge);
    if (error != kErrorNone)
    {
        if (mMode == kSelectedParent)
        {
            LogWarn(\"SelectedParent response challenge mismatch err=%s keyseq=%lu frame=%lu\", ErrorToString(error),
                    ToUlong(aRxInfo.mKeySequence), ToUlong(aRxInfo.mFrameCounter));
        }
        ExitNow();
    }
""",
    ),
    (
        ROOT / "thread/mle.cpp",
        """    aRxInfo.mClass = RxInfo::kAuthoritativeMessage;

#if OPENTHREAD_FTD
""",
        """    aRxInfo.mClass = RxInfo::kAuthoritativeMessage;

    if (mMode == kSelectedParent)
    {
        LogNote(\"SelectedParent ParentResponse rx src=0x%04x rss=%d mode=%u\", sourceAddress, rss, mMode);
    }

#if OPENTHREAD_FTD
""",
    ),
    (
        ROOT / "thread/mle.cpp",
        """    VerifyOrExit(aRxInfo.IsNeighborStateValid(), error = kErrorSecurity);

    VerifyOrExit(mState == kStateChildIdRequest);
""",
        """    if ((mMode == kSelectedParent) && !aRxInfo.IsNeighborStateValid())
    {
        LogWarn(\"SelectedParent ChildIdResponse neighbor invalid src=0x%04x keyseq=%lu frame=%lu\", sourceAddress,
                ToUlong(aRxInfo.mKeySequence), ToUlong(aRxInfo.mFrameCounter));
        ExitNow(error = kErrorSecurity);
    }

    VerifyOrExit(aRxInfo.IsNeighborStateValid(), error = kErrorSecurity);

    VerifyOrExit(mState == kStateChildIdRequest);
""",
    ),
    (
        ROOT / "thread/mle.cpp",
        """exit:
    LogProcessError(kTypeChildIdResponse, error);
}
""",
        """exit:
    if ((error != kErrorNone) && (mMode == kSelectedParent))
    {
        LogWarn(\"SelectedParent ChildIdResponse reject err=%s src=0x%04x short=0x%04x\", ErrorToString(error),
                sourceAddress, shortAddress);
    }

    LogProcessError(kTypeChildIdResponse, error);
}
""",
    ),
    (
        ROOT / "thread/mle.cpp",
        """exit:
    LogProcessError(kTypeParentResponse, error);
}
""",
        """exit:
    if ((error != kErrorNone) && (mMode == kSelectedParent))
    {
        LogWarn(\"SelectedParent ParentResponse reject err=%s src=0x%04x rss=%d cand=0x%04x\", ErrorToString(error),
                sourceAddress, rss, mParentCandidate.GetRloc16());
    }

    LogProcessError(kTypeParentResponse, error);
}
""",
    ),
    (
        ROOT / "api/thread_api.cpp",
        """otError otThreadSearchForBetterParent(otInstance *aInstance)
{
    return AsCoreType(aInstance).Get<Mle::Mle>().SearchForBetterParent();
}
""",
        """otError otThreadSearchForBetterParent(otInstance *aInstance)
{
    return AsCoreType(aInstance).Get<Mle::Mle>().SearchForBetterParent();
}

extern \"C\" bool biparental_ot_request_selected_parent_attach(otInstance *aInstance,
                                                               const otExtAddress *aPreferredExtAddress)
{
    if ((aInstance == nullptr) || (aPreferredExtAddress == nullptr))
    {
        return false;
    }

    return AsCoreType(aInstance).Get<Mle::Mle>().AttachToSelectedParent(AsCoreType(aPreferredExtAddress)) ==
           kErrorNone;
}
""",
    ),
]


def patch_once(path: Path, old: str, new: str) -> str:
    text = path.read_text()
    if new in text:
      return "already"
    if old not in text:
      return "missing"
    path.write_text(text.replace(old, new, 1))
    return "patched"


def main() -> int:
    rc = 0
    for path, old, new in PATCHES:
        if not path.exists():
            print(f"[missing-file] {path}")
            rc = 1
            continue
        state = patch_once(path, old, new)
        print(f"[{state}] {path}")
        if state == "missing":
            rc = 1
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
