import {
  IconMenu2,
  IconCheck,
  IconDownload,
  IconTag,
  IconChevronRight,
  IconClockHour4,
  IconX,
  IconBooks,
  IconTelescope,
  IconHexagons,
  IconSearch,
  IconRobot,
  IconBinaryTree,
  IconBulb,
  IconBug,
  IconTags,
  IconWand,
  IconDiamond,
  IconCommand,
  IconEdit,
  IconTools,
  IconBrain,
} from "@tabler/icons";

import { AiFillApple } from "@react-icons/all-files/ai/AiFillApple";
import { AiFillWindows } from "@react-icons/all-files/ai/AiFillWindows";
import { SiVim } from "@react-icons/all-files/si/SiVim";
import { FaLinux } from "@react-icons/all-files/fa/FaLinux";
import { FaTwitter } from "@react-icons/all-files/fa/FaTwitter";
import { FaDiscord } from "@react-icons/all-files/fa/FaDiscord";

import posthog from "posthog-js";
import cx from "classnames";
import React from "react";
import { Helmet } from "react-helmet";
import { twMerge } from "tailwind-merge";

import {
  BrowserRouter,
  Link,
  Outlet,
  Navigate,
  Route,
  Routes,
  useLocation,
} from "react-router-dom";
import _ from "lodash";
import "./index.css";

posthog.init("phc_kIt8VSMD8I2ScNhnjWDU2NmrK9kLIL3cHWpkgCX3Blw", {
  api_host: "https://app.posthog.com",
});

const SUPPORT_EMAIL = "support@codeperfect95.com";
const CURRENT_BUILD = process.env.REACT_APP_BUILD_VERSION;
const CURRENT_BUILD_RELEASE_DATE = "February 12, 2023";

const isDev = process.env.REACT_APP_CPENV === "development";
const isStaging = process.env.REACT_APP_CPENV === "staging";

const BASE_LINKS = {
  docs: "https://docs.codeperfect95.com",
  gettingStarted: "https://docs.codeperfect95.com/getting-started",
  changelog: "https://docs.codeperfect95.com/changelog",
  issueTracker: "https://github.com/codeperfect95/issue-tracker",

  buyPersonalMonthly: "https://buy.stripe.com/aEU5kx2aTaso4TK008",
  buyPersonalYearly: "https://buy.stripe.com/fZefZb2aTdEAbi8aEN",
  buyProMonthly: "https://buy.stripe.com/6oE28ldTB5843PG9AK",
  buyProYearly: "https://buy.stripe.com/28o8wJ3eXfMI4TK5kv",

  twitter: "https://twitter.com/codeperfect95",
  discord: "https://discord.gg/WkFY44BY7a",
  substack: "https://codeperfect95.substack.com/",
  handmadeManifesto: "https://handmade.network/manifesto",
  nonPessimized: "https://www.youtube.com/watch?v=pgoetgxecw8",
  mikeActon: "https://www.youtube.com/watch?v=rX0ItVEVjHc",
  roadAhead: "https://codeperfect95.substack.com/p/the-road-ahead",
  oldSoftwareOpenedInstantly:
    "https://www.youtube.com/watch?v=GC-0tCy4P1U&t=2168s",
};

const DEV_LINKS = {
  buyPersonalMonthly: "https://buy.stripe.com/test_4gw8xrb10g8D7QsbIP",
  buyPersonalYearly: "https://buy.stripe.com/test_8wMfZT3yy2hN1s45ks",
  buyProMonthly: "https://buy.stripe.com/test_6oEdRL1qq5tZ5Ik6oy",
  buyProYearly: "https://buy.stripe.com/test_3cs6pj9WW3lR3Ac7sB",
};

const STAGING_LINKS = {
  docs: "https://dev-docs.codeperfect95.com",
  gettingStarted: "https://dev-docs.codeperfect95.com/getting-started",
  changelog: "https://dev-docs.codeperfect95.com/changelog",
};

const LINKS = {
  ...BASE_LINKS,
  ...(isDev ? DEV_LINKS : {}),
  ...(isStaging ? STAGING_LINKS : {}),
};

function asset(path) {
  return isDev ? `/public${path}` : path;
}

function isExternalLink(href) {
  const prefixes = ["http://", "https://", "mailto:", "tel:"];
  return prefixes.some((it) => href.startsWith(it));
}

function A({ children, href, newWindow, ...props }) {
  if (href && !isExternalLink(href) && !newWindow) {
    props.to = href;
    return <Link {...props}>{children}</Link>;
  }

  let useNewWindow;
  if (newWindow) {
    useNewWindow = true;
  } else if (newWindow === false) {
    useNewWindow = false;
  } else if (isExternalLink(href) && !href.startsWith("mailto:")) {
    useNewWindow = true;
  }

  if (useNewWindow) {
    props.target = "_blank";
  }
  props.href = href;
  return <a {...props}>{children}</a>;
}

function wrap(elem, extraClassName, defaultProps, overrideProps) {
  return ({ children, className, ...props }) => {
    const newProps = {
      ...(defaultProps || {}),
      ...props,
      ...(overrideProps || {}),
      className: twMerge(extraClassName, className),
    };
    return React.createElement(elem, newProps, children);
  };
}

const WallOfText = wrap(
  "div",
  "prose leading-normal md:mx-auto md:max-w-2xl bg-white md:my-32 p-8 md:p-16 md:rounded-lg md:shadow-sm text-neutral-700"
);
const Title = wrap("h2", "m-0 mb-6 title text-3xl");

function Icon({ block, noshift, icon: IconComponent, ...props }) {
  return (
    <span className={block ? "block" : "inline-block"}>
      <IconComponent {...props} />
    </span>
  );
}

const FEATURES = _.shuffle([
  {
    label: "Integrated Debugger",
    desc: "Full Delve integration gives you full debugging powers.",
    icon: IconBug,
  },
  {
    label: "Code Intelligence",
    desc: "Go to definition, find all usages, parameter hints, autocomplete, all the table stakes.",
    icon: IconBulb,
  },
  {
    label: "Smart Autocomplete",
    desc: "Context-specific suggestions as you type. ",
    icon: IconBrain,
  },
  {
    label: "Postfix Completions",
    desc: "Macros that work intelligently on your Go expressions.",
    icon: IconRobot,
  },
  {
    label: "Global Fuzzy Selector",
    desc: "Works on files, symbols, commands, and completions.",
    icon: IconTelescope,
  },
  {
    label: "Tree-Based Navigation",
    desc: "Use the power of our integrated parser to directly walk the AST.",
    icon: IconBinaryTree,
  },
  {
    label: "Auto Format",
    desc: "Automatically format your code on save, with zero configuration.",
    icon: IconWand,
  },
  {
    label: "Integrated Build",
    desc: "Build and jump to/fix each error with ergonomic shortcuts.",
    icon: IconTools,
  },
  {
    label: "Rename Identifier",
    desc: "Rename any symbol across your entire codebase.",
    icon: IconEdit,
  },
  {
    label: "Command Palette",
    desc: "Press Primary+K to run any command or action.",
    icon: IconCommand,
  },
  {
    label: "Generate Function",
    desc: "Take a call to a non-existent function and generate its signature.",
    icon: IconDiamond,
  },
  {
    label: "Fast Project-Wide Grep",
    desc: "Fast search (and replace) runs in milliseconds on large codebases.",
    icon: IconSearch,
  },
  {
    label: "Manage Interfaces",
    desc: "Find implementations and interfaces and generate implementations.",
    icon: IconHexagons,
  },
  {
    label: "Organize Imports",
    desc: "Intelligently add/remove imports with our native import organizer.",
    icon: IconBooks,
  },
  {
    label: "Manage Struct Tags",
    desc: "Add/remove struct tags automatically.",
    icon: IconTags,
  },
  {
    label: "Vim Integration",
    desc: "First-class Vim support, integrated with everything else.",
    icon: SiVim,
  },
]);

const BAD_FEATURES = [
  "Electron",
  "JavaScript",
  "language servers",
  "slow runtimes",
  "bulky frameworks",
  "bloated abstractions",
];

function Home() {
  return (
    <div className="bg-neutral-50 mx-auto md:pt-12 w-full">
      <div className="max-w-full leading-relaxed px-8 py-12 md:py-8 md:pb-20">
        <div className="md:text-center font-bold text-5xl md:text-5xl mb-1 md:mb-2 text-black tracking-tight leading-[1.1] md:leading-[1.1]">
          A fast, lightweight IDE for Go
        </div>
        <div className="md:text-center text-[130%] text-gray-600 leading-normal">
          A power tool with a small resource footprint.
        </div>
        <div className="mt-8 md:mt-8 md:text-center flex gap-4 md:justify-center">
          <A
            href="/download"
            className="btn btn1 justify-center flex md:inline-flex text-center"
          >
            <Icon size={18} className="mr-1" icon={IconDownload} />
            Download
          </A>
          <A
            href={LINKS.docs}
            className="btn btn2 justify-center flex md:inline-flex text-center"
          >
            View Docs
            <Icon size={18} className="ml-1" icon={IconChevronRight} />
          </A>
        </div>
      </div>

      <div className="bg-neutral-900 border-gray-100 p-8 py-12 md:pb-24 relative z-10 flex items-center justify-center">
        <div className="max-w-screen-2xl flex flex-col lg:flex-row items-center gap-8 md:gap-16">
          <div className="lg:w-1/3 md:mx-0">
            <div className="text-[160%] md:text-[225%] font-semibold text-black leading-tight">
              {BAD_FEATURES.map((name) => (
                <div className="text-white md:whitespace-nowrap">
                  No {name}.
                </div>
              ))}
            </div>
            <div className="text-lg leading-normal mt-6 md:mt-8 text-neutral-400">
              <p>
                We threw out the modern software stack and rewrote the entire
                IDE with in blazing fast C/C++.
              </p>
              <p>
                Performance as fast as a video game. Instant startup. 144 FPS.
                No latency between keystrokes. An indexer that gobbles through
                large codebases.
              </p>
              <p>
                With predictable operations, ergonomic shortcuts, and a
                streamlined workflow, CodePerfect "just does the right thing"
                and gets out of the way.
              </p>
            </div>
          </div>
          <div className="md:flex-1">
            <img
              className="max-w-full shadow-lg rounded-lg overflow-hidden"
              alt="screenshot"
              src={asset("/download.png")}
            />
          </div>
        </div>
      </div>

      <div className="batteries-included z-10 px-4 md:px-12 py-12 md:py-20">
        <h1 className="title text-3xl md:text-4xl mb-6 md:mb-12 text-center">
          <div className="block md:inline-block">Batteries included,</div>
          <span className="hidden md:inline-block">&nbsp;</span>
          <div className="block md:inline-block">zero configuration.</div>
        </h1>
        <div className="max-w-screen-lg md:gap-x-12 mx-auto">
          <div className="grid grid-cols-2 md:grid-cols-4 gap-3 md:gap-6 mt-4 md:mt-0 md:p-0">
            {FEATURES.map((it) => (
              <div
                className="bg-white hover:scale-[103%] shadow-sm rounded p-3 md:p-4 transition-all select-none"
                key={it.label}
              >
                <Icon stroke={1.25} size={26} icon={it.icon} />
                <div className="title leading-none text-neutral-700 mt-0.5 md:mt-0.5">
                  {it.label}
                </div>
                <div className="mt-2 font-sans text-neutral-500 leading-snug">
                  {it.desc}
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>

      <div className="md:text-center px-6 py-12 md:py-16 bg-white">
        <div className="title text-3xl md:text-4xl mb-3">
          Ready to get started?
        </div>
        <div className="mx-auto text-lg md:text-xl leading-relaxed mb-6 md:mb-12 text-neutral-600">
          Try CodePerfect for free for 7 days with all features available.
        </div>
        <A
          href="/download"
          className={twMerge(
            "btn btn1 btn-lg justify-center inline-flex text-center bg-black text-white px-6"
          )}
        >
          <Icon className="mr-2" icon={IconDownload} size={20} />
          Download
        </A>
      </div>
    </div>
  );
}

function PaymentDone() {
  return (
    <WallOfText>
      <Title>Thanks for the purchase!</Title>
      <p>
        Please check your email &mdash; we just sent your license key to you. If
        you don't see it, see if it's in your spam folder. If you still don't
        see it, please <a href={`mailto:${SUPPORT_EMAIL}`}>contact support</a>.
      </p>
      <p>
        If you haven't done so yet, you can download CodePerfect{" "}
        <A href="/download">here</A>.
      </p>
    </WallOfText>
  );
}

function PortalDone() {
  return (
    <WallOfText>
      <p className="text-center">
        You just left the billing portal. You can now close this window.
      </p>
    </WallOfText>
  );
}

function WithTooltip({ className, label, show, children, bottom }) {
  console.log(bottom);
  return (
    <div className={cx("relative group", className)}>
      {show && (
        <span
          className={cx(
            "hidden group-hover:inline-block text-sm font-title font-semibold shadow absolute",
            "rounded bg-neutral-800 text-neutral-200 py-2 px-3 leading-none whitespace-nowrap w-auto",
            !bottom && "bottom-full mb-4 right-1/2 translate-x-1/2",
            bottom && "top-full mt-4 right-1/2 translate-x-1/2"
          )}
        >
          <span
            class={cx(
              "w-0 h-0 border-[6px] border-transparent absolute left-1/2 -translate-x-1/2",
              !bottom && "border-t-neutral-800 top-full",
              bottom && "border-b-neutral-800 bottom-full"
            )}
          />
          {label}
        </span>
      )}
      {children}
    </div>
  );
}

function BuyLicense() {
  const plans = [
    {
      name: "Personal",
      price: 5,
      features: [
        { label: "Commercial use ok" },
        { label: "All features available" },
        { not: true, label: "Company can't pay" },
        { not: true, label: "Purchase can't be expensed" },
      ],
      buttons: {
        monthlyLink: LINKS.buyPersonalMonthly,
        yearlyLink: LINKS.buyPersonalYearly,
      },
    },

    {
      name: "Pro",
      price: 10,
      features: [
        { label: "Commercial use ok" },
        { label: "All features available" },
        { label: "Company can pay*" },
        { label: "Purchase can be expensed" },
      ],
      buttons: {
        monthlyLink: LINKS.buyProMonthly,
        yearlyLink: LINKS.buyProYearly,
      },
    },

    {
      name: "Enterprise",
      price: "custom",
      features: [
        { label: "Priority support" },
        { label: "Custom billing & pricing" },
        { label: "Team licensing & management" },
        { label: "Other custom requests" },
      ],
      buttons: { enterprise: true },
    },
  ];

  const help = [
    {
      title: "Not sure yet?",
      content: (
        <>
          <p>
            You can try CodePerfect for free for 7 days, no restrictions. No
            credit card required.
          </p>
          <p>
            <A href="/download" className="btn btn1 btn-sm">
              Visit Downloads
            </A>
          </p>
        </>
      ),
    },
    {
      title: "Buying as a team?",
      content: (
        <>
          <p>
            If you need multiple licenses, bulk pricing, team management, or
            have any other custom requests, please reach out.
          </p>
          <p>
            <A href={`mailto:${SUPPORT_EMAIL}`} className="btn btn2 btn-sm">
              Get in touch
            </A>
          </p>
        </>
      ),
    },
    {
      title: "Have another question?",
      content: (
        <>
          <p>
            Please get in touch with us and we'll respond within 1 business day.
          </p>
          <p>
            <A href={`mailto:${SUPPORT_EMAIL}`} className="btn btn2 btn-sm">
              Contact us
            </A>
          </p>
        </>
      ),
    },
  ];

  const planOptions = (buttons) => [
    [buttons.monthlyLink, "monthly", false],
    [buttons.yearlyLink, "yearly", true],
  ];

  return (
    <div className="mt-12 md:mt-24">
      <div className="title text-center text-3xl md:text-5xl mb-6 md:mb-12">
        Buy License
      </div>
      <div className="md:max-w-screen-xl mx-auto">
        <div className="mx-6 my-6 grid grid-cols-1 md:grid-cols-3 gap-4 md:gap-6 rounded">
          {plans.map(({ name, price, features, buttons, recommended }) => {
            return (
              <div className="shadow rounded-lg bg-white">
                <div className="p-5">
                  <div className="text-neutral-300">
                    <div className="font-title font-medium text-neutral-500">
                      {name}
                    </div>
                    <div className="title text-xl">
                      {price === "custom" ? "Custom pricing" : <>${price}/mo</>}
                    </div>
                  </div>
                  <div className="my-5">
                    {features.map((it) => (
                      <div
                        className={cx(
                          "flex items-center space-x-1.5 leading-6 mb-1 last:mb-0",
                          it.not ? "text-red-400" : "text-neutral-600"
                        )}
                      >
                        <Icon size={18} icon={it.not ? IconX : IconCheck} />
                        <span>{it.label}</span>
                      </div>
                    ))}
                  </div>
                  <div>
                    {buttons.enterprise ? (
                      <A
                        className="btn btn2 block text-center py-3 border border-neutral-200 shadow-sm"
                        href={`mailto:${SUPPORT_EMAIL}`}
                      >
                        Contact support
                      </A>
                    ) : (
                      <div className="flex flex-col md:grid md:grid-cols-2 items-center gap-2">
                        {planOptions(buttons).map(
                          ([link, unit, yearly], index) => (
                            <WithTooltip
                              className="block w-full md:w-auto md:inline-block"
                              label="2 months free!"
                              show={yearly}
                            >
                              <A
                                className="btn btn1 py-3 block text-center"
                                href={link}
                              >
                                Buy {unit}
                              </A>
                            </WithTooltip>
                          )
                        )}
                      </div>
                    )}
                  </div>
                </div>
              </div>
            );
          })}
        </div>
        <div className="text-neutral-600 mt-4 mx-6 md:mx-0 md:text-center leading-tight text-base">
          * Many users expense the purchase with their employer.
        </div>
      </div>
      <div className="md:max-w-screen-xl mx-auto">
        <div className="mx-6 grid grid-cols-1 md:grid-cols-3 my-12 md:mb-16 gap-12 md:gap-0">
          {help.map((it) => (
            <div className="md:border-y md:border-l md:border-r-0 md:last:border-r md:border-gray-200 md:first:border-l md:border-dashed md:p-8">
              <div className="text-neutral-700 text-lg font-semibold mb-2">
                {it.title}
              </div>
              <div className="text-gray-600 leading-normal">{it.content}</div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}

const faqs = [
  {
    q: "What makes CodePerfect fast?",
    a: (
      <>
        <p>
          There isn't any one thing or algorithm or technique. CodePerfect is
          fast compared to modern software because it declines to copy the
          numerous things that make modern software slow.
        </p>

        <p>
          The modern tech stack has extreme bloat everywhere. We eschewed that
          and instead use a low level language (C/C++) and manage our own memory
          with amortized bulk arena allocation. We write straightforward,{" "}
          <A href={LINKS.nonPessimized}>non-pessimized</A> code that just
          executes the actual CPU instructions that do the thing it's supposed
          to.
        </p>

        <p>
          We're not writing crazy inline assembly or SIMD intrinsics or
          whatever. We do some optimization, like using file mappings and
          multithreading stuff where it makes sense, but mostly we are just
          writing straightforward code that performs the actual task of
          executing an IDE. Modern computers are just fast.
        </p>
      </>
    ),
  },
  {
    q: "How does CodePerfect compare with Jetbrains and VSCode?",
    a: (
      <>
        <p>
          Jetbrains has more features, VSCode is free and customizable, and
          CodePerfect is fast.
        </p>

        <p>
          Right now we're targeting people who want a code editor as fast as Vim
          or Sublime Text, but comes with code intelligence and other IDE
          features you need to program productively. Our users spend substantial
          amounts of time in their editor, and derive significant value and joy
          from a seamless, latency-free workflow.
        </p>
      </>
    ),
  },
  {
    q: "What's the long term goal?",
    a: (
      <>
        <p>
          We are trying to build a custom power tool for the specific task of
          programming.
        </p>
        <p>
          New programming tools today tend to have ambitious goals like making
          programming more collaborative, or involve less code, or more
          integrated with third-party tools. We're aiming in a boring orthogonal
          direction: building the best tool for literally editing, compiling,
          and debugging code. CodePerfect is tightly integrated and optimized
          around that use case.
        </p>

        <p>
          We want to be <A href="https://sesuperhuman.com">Superhuman</A> for
          programming. Programmers use their IDE all day, and small improvements
          add up. We want to make big improvements.
        </p>
        <p>
          A big part of this is building a smooth experience. But up to a point,
          smoothness means speed (or low latency), so that's a big initial
          focus.
        </p>
      </>
    ),
  },
  {
    q: "Why is it a recurring subscription?",
    a: (
      <>
        <p>
          Because we have to pay our rent, which is a recurring subscription :)
        </p>
        <p>
          CodePerfect is also in{" "}
          <A href={LINKS.changelog}>active development</A> so technically you
          are paying for constant new updates.
        </p>
        <p>
          We understand some users adamantly oppose subscription models and
          require a perpetual license. We sell one for the cost of four years:
          $200 for individuals, $400 if you're expensing. Please{" "}
          <A href={`mailto:${SUPPORT_EMAIL}`}>email us</A> to initiate this
          process.
        </p>
      </>
    ),
  },
  {
    q: "What does the name mean?",
    a: (
      <>
        <p>
          It's a throwback to an era when software was way{" "}
          <A href={LINKS.oldSoftwareOpenedInstantly}>faster</A>, despite running
          on hardware orders of magnitude slower than a phone today.
        </p>
      </>
    ),
  },
];

function FAQ() {
  return (
    <div className="bg-white md:bg-transparent py-12 px-6 md:px-4 md:py-24 md:max-w-screen-sm mx-auto">
      <div className="md:px-4 md:text-center text-3xl md:text-5xl title mb-8">
        FAQ
      </div>
      <div>
        {faqs.map((it) => (
          <div
            className="prose md:p-6 md:border-0 mb-12 md:mb-4 last:mb-0 md:bg-white md:rounded-lg md:shadow-sm"
            key={it.q}
          >
            <p className="text-lg font-bold">{it.q}</p>
            {it.a}
          </div>
        ))}
      </div>
    </div>
  );
}

function Download() {
  const links = [
    {
      platform: "windows-x64",
      icon: AiFillWindows,
      label: "Windows",
      disabledText: "Temporarily unavailable.",
    },
    {
      platform: "mac-x64",
      icon: AiFillApple,
      label: "macOS Intel",
    },
    {
      platform: "mac-arm",
      icon: AiFillApple,
      label: "macOS M1",
    },
    {
      platform: "linux-x64",
      icon: FaLinux,
      label: "Linux",
      disabledText: "Coming soon!",
    },
  ];

  return (
    <div className="my-12 md:my-28 px-6 md:px-0">
      <div className="md:px-4 md:text-center text-3xl md:text-5xl title leading-none">
        Download CodePerfect
      </div>
      <div className="text-center mt-2">
        <A
          href={`${LINKS.changelog}/${CURRENT_BUILD}`}
          className={cx(
            "flex items-start md:items:center md:justify-center flex-col md:flex-row",
            "gap-1 md:gap-4 no-underline opacity-90 hover:opacity-100 font-ui"
          )}
        >
          <span className="inline-block">
            <span className="font-semibold text-sm text-neutral-700 flex gap-x-1.5 items-center">
              <Icon size={18} icon={IconTag} />
              <span>Build {CURRENT_BUILD}</span>
            </span>
          </span>
          <span className="inline-block">
            <span className="font-semibold text-sm text-neutral-700 gap-x-1.5 items-center flex">
              <Icon size={18} icon={IconClockHour4} />
              <span>Released {CURRENT_BUILD_RELEASE_DATE}</span>
            </span>
          </span>
        </A>
      </div>
      <div className="mt-8 md:text-center">
        <p className="md:max-w-xs mx-auto flex flex-wrap flex-col justify-center">
          {links.map((it, index) => (
            <div className="mb-3 md:mb-2">
              <WithTooltip
                className="w-full h-full"
                show={it.disabledText}
                label={it.disabledText}
                bottom={index === links.length - 1}
              >
                <A
                  href={
                    it.disabledText
                      ? "#"
                      : `https://codeperfect95.s3.us-east-2.amazonaws.com/app/${it.platform}-${CURRENT_BUILD}.zip`
                  }
                  className={cx(
                    "btn btn1 flex leading-none py-4 px-5 relative",
                    it.disabledText && "disabled"
                  )}
                  onClick={() => {
                    posthog.capture("download", { platform: it.platform });
                  }}
                >
                  <Icon
                    size={18}
                    stroke={2}
                    className="relative mt-0.5 mr-1"
                    icon={it.icon}
                  />
                  <span>{it.label}</span>
                </A>
              </WithTooltip>
            </div>
          ))}
        </p>
        <div className="flex items-center justify-center">
          <div className="max-w-screen-xl mt-4 flex items-start md:items-center gap-2">
            <span>
              (CodePerfect is free to evaluate for 7 days. After that you'll
              need a <A href="/buy">license</A> to keep using it.)
            </span>
          </div>
        </div>
      </div>
    </div>
  );
}

function Terms() {
  return (
    <WallOfText>
      <Title>Terms of Service</Title>
      <p>
        This website provides you with information about the IDE and a means for
        you to subscribe to our services, which allow you to use the IDE for as
        long as your subscription is active.
      </p>
      <p>
        The IDE is an application that lets you write Go applications. In
        exchange for paying a monthly rate, we provide you with a license to use
        it.
      </p>

      <div className="mt-16">
        <Title>Privacy Policy</Title>
        <p>
          When you sign up, we collect your name, email, and credit card
          information. We use this information to bill you and send you emails
          with updates about your payment status (for example, if your card
          fails).
        </p>
        <p>
          The IDE contacts the server to authenticate your license key and to
          install automatic updates. This exposes your IP address to us. We
          won't share it with anyone, unless ordered to by law.
        </p>
      </div>
    </WallOfText>
  );
}

function ScrollToTop() {
  const { pathname, hash } = useLocation();

  React.useEffect(() => {
    if (!hash) {
      window.scrollTo(0, 0);
    }
  }, [pathname, hash]);

  return null;
}

function Logo({ onClick, hideText }) {
  return (
    <A
      href="/"
      className="font-bold text-lg text-black no-underline whitespace-nowrap inline-flex flex-shrink-0 items-center"
      onClick={onClick}
    >
      <img
        alt="logo"
        className="w-auto h-8 inline-block mr-3"
        src={asset("/logo.png")}
      />
      {!hideText && (
        <span className="inline-block logo text-lg font-bold">
          CodePerfect 95
        </span>
      )}
    </A>
  );
}

function Header() {
  const [showMenu, setShowMenu] = React.useState(false);

  React.useEffect(() => {
    const listener = (e) => {
      setShowMenu(false);
    };
    document.body.addEventListener("click", listener);
    return () => document.body.removeEventListener("click", listener);
  }, []);

  const links = [
    [LINKS.docs, "Docs"],
    [LINKS.changelog, "Changelog"],
    [LINKS.discord, "Discord"],
    ["/faq", "FAQ"],
    ["/buy", "Buy"],
    ["/download", "Download"],
  ];

  return (
    <div className="p-4 md:p-4 border-b border-gray-50 bg-white">
      <div className="flex justify-between items-center w-full md:max-w-screen-lg md:mx-auto text-lg">
        <Logo />
        <div className="md:hidden relative">
          <Icon
            block
            onClick={(e) => {
              setShowMenu(!showMenu);
              e.stopPropagation();
            }}
            className="ml-2 text-3xl leading-none opacity-50"
            icon={IconMenu2}
          />
          {showMenu && (
            <div
              className="bg-neutral-900 text-black fixed top-0 left-0 right-0 p-4 border-b border-gray-200 shadow-lg z-50"
              onClick={(e) => e.stopPropagation()}
            >
              <button
                onClick={() => setShowMenu(false)}
                className="absolute z-50 top-4 right-4 w-8 flex items-center justify-center h-8 rounded-full bg-neutral-700 text-white"
              >
                <Icon icon={IconX} />
              </button>
              <div className="invert z-40 relative">
                <Logo onClick={() => setShowMenu(false)} />
              </div>
              <div className="mt-2">
                {links.map(([url, label]) => (
                  <A
                    key={url}
                    className="block text-neutral-100 no-underline whitespace-nowrap md:hidden leading-none py-2"
                    onClick={() => setShowMenu(false)}
                    href={url}
                  >
                    {label}
                  </A>
                ))}
              </div>
            </div>
          )}
        </div>
        <div className="hidden md:flex items-baseline gap-x-8">
          {links.map(([url, label]) => (
            <A
              key={url}
              className="text-[95%] text-neutral-700 no-underline whitespace-nowrap hidden md:inline-block"
              href={url}
            >
              {label}
            </A>
          ))}
        </div>
      </div>
    </div>
  );
}

const FootSection = wrap("div", "flex flex-col gap-y-3 md:gap-y-3 text-left");
const FootLink = wrap(A, "text-gray-800 no-underline");

function Footer() {
  return (
    <div className="bg-white px-6 pt-6 lg:pt-12 pb-8 md:pb-24 border-t border-gray-50">
      <div className="flex flex-col-reverse md:flex-row gap-y-4 md:gap-0 hmd:flex-row justify-between w-full md:max-w-screen-xl md:mx-auto items-start">
        <div className="text-gray-500">
          <div className="opacity-50 hidden md:block">
            <Logo hideText />
          </div>
          <div>&copy; {new Date().getFullYear()} CodePerfect 95</div>
        </div>
        <div className="flex flex-col md:flex-row md:items-start gap-y-3 md:gap-x-14 leading-none">
          <FootSection>
            <FootLink href="/buy">Buy License</FootLink>
            <FootLink href="/download">Download</FootLink>
          </FootSection>
          <FootSection>
            <FootLink href={LINKS.docs}>Docs</FootLink>
            <FootLink href={LINKS.changelog}>Changelog</FootLink>
            <FootLink href={LINKS.issueTracker}>Issue Tracker</FootLink>
            <FootLink href={LINKS.substack}>Substack</FootLink>
          </FootSection>
          <FootSection>
            <FootLink href={`mailto:${SUPPORT_EMAIL}`}>Support</FootLink>
            <FootLink href="/faq">FAQ</FootLink>
            <FootLink href="/terms">Terms &amp; Privacy</FootLink>
          </FootSection>
          <div className="flex gap-x-4 text-2xl mt-3 md:mt-0">
            <A className="text-gray-800" href={LINKS.discord}>
              <Icon block icon={FaDiscord} />
            </A>
            <A className="text-gray-800" href={LINKS.twitter}>
              <Icon block icon={FaTwitter} />
            </A>
          </div>
        </div>
      </div>
    </div>
  );
}

function Layout() {
  return (
    <div className="flex flex-col min-h-screen">
      <div className="flex-grow">
        <ScrollToTop />
        <Header />
        <Outlet />
      </div>
      <Footer />
    </div>
  );
}

function App() {
  return (
    <>
      <Helmet>
        <meta charSet="utf-8" />
        <title>CodePerfect 95</title>
      </Helmet>
      <BrowserRouter>
        <Routes>
          <Route path="/" element={<Layout />}>
            <Route path="download" element={<Download />} />
            <Route path="faq" element={<FAQ />} />
            <Route path="buy" element={<BuyLicense />} />
            <Route path="payment-done" element={<PaymentDone />} />
            <Route path="portal-done" element={<PortalDone />} />
            <Route path="terms" element={<Terms />} />
            <Route path="privacy" element={<Navigate to="terms" />} />
            <Route path="/" element={<Home />} />
            <Route path="*" element={<Navigate to="/" />} />
          </Route>
        </Routes>
      </BrowserRouter>
    </>
  );
}

export default App;
