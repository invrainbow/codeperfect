import {
  IconMenu2,
  IconCheck,
  IconDownload,
  IconTag,
  IconChevronRight,
  IconChevronDown,
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
  IconKeyboard,
  IconBike,
  IconCategory2,
} from "@tabler/icons";

import { AiFillApple } from "@react-icons/all-files/ai/AiFillApple";
import { AiFillWindows } from "@react-icons/all-files/ai/AiFillWindows";
import { FaLinux } from "@react-icons/all-files/fa/FaLinux";
import { FaTwitter } from "@react-icons/all-files/fa/FaTwitter";
import { FaDiscord } from "@react-icons/all-files/fa/FaDiscord";

import posthog from "posthog-js";
import cx from "classnames";
import React from "react";
import { Helmet } from "react-helmet";
// import { useMediaQuery } from "react-responsive";
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
const CURRENT_BUILD_RELEASE_DATE = "November 20, 2022";

const isDev = process.env.NODE_ENV === "development";

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
  mailingList: "https://exceptional-innovator-7731.ck.page/0b59fe1347",
  handmadeManifesto: "https://handmade.network/manifesto",
};

const DEV_LINKS = {
  buyPersonalMonthly: "https://buy.stripe.com/test_4gw8xrb10g8D7QsbIP",
  buyPersonalYearly: "https://buy.stripe.com/test_8wMfZT3yy2hN1s45ks",
  buyProMonthly: "https://buy.stripe.com/test_6oEdRL1qq5tZ5Ik6oy",
  buyProYearly: "https://buy.stripe.com/test_3cs6pj9WW3lR3Ac7sB",
};

function makeMergedLinks() {
  const mergedLinks = { ...BASE_LINKS };
  if (isDev) {
    _.merge(mergedLinks, DEV_LINKS);
  }
  return mergedLinks;
}

const LINKS = makeMergedLinks();

const CDN_PATH = "https://codeperfect-static.s3.us-east-2.amazonaws.com";

function asset(path) {
  return isDev ? `/public${path}` : `${CDN_PATH}${path}`;
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

function Image({ src, ...rest }) {
  const imgProps = {
    alt: "image",
    src,
    ...rest,
  };

  return (
    <>
      <A newWindow href={src} className="md:hidden">
        <img {...imgProps} />
      </A>
      <span className="hidden md:block">
        <img {...imgProps} />
      </span>
    </>
  );
}

const WallOfText = wrap(
  "div",
  "wall-of-text leading-normal md:mx-auto md:max-w-2xl bg-white md:my-32 p-8 md:p-16 md:rounded-lg md:shadow text-neutral-700"
);
const Title = wrap("h2", "m-0 mb-6 font-bold text-3xl text-black font-title");

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
    desc: "Full Delve integration lets you full debugging powers.",
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
    desc: "Automatically gofmt your code on save, with zero configuration.",
    icon: IconWand,
  },
  {
    label: "Integrated Build",
    desc: "Build, jump to & fix each error, all with ergonomic keyboard shortcuts.",
    icon: IconTools,
  },
  {
    label: "Rename Identifier",
    desc: "Rename any symbol across your entire codebase.",
    icon: IconEdit,
  },
  {
    label: "Command Palette",
    desc: "Press Primary+K to run any command or action inside CodePerfect.",
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
    desc: "Add & remove struct tags automatically.",
    icon: IconTags,
  },
]);

const SELLING_POINTS = [
  {
    icon: IconKeyboard,
    icon2: asset("/icon-vim.png"),
    image: asset("/vim.avif"),
    label: "Full native Vim integration",
    content: (
      <>
        <p>
          First-class Vim support &mdash; not a plugin as an afterthought. The
          whole Vim feature set, integrated seamlessly with everything else.
        </p>
      </>
    ),
  },
  {
    icon: IconCategory2,
    icon2: asset("/icon-basics.png"),
    label: "Back to basics",
    content: (
      <>
        <p>
          Straightforward,{" "}
          <A href="https://www.youtube.com/watch?v=pgoetgxecw8">
            non-pessimized
          </A>{" "}
          code brings you: Instant startup. A buttery 144 frames/sec. Near-zero
          latency between keystrokes. An indexer that gobbles through large
          codebases.
        </p>
      </>
    ),
  },
  {
    icon: IconBike,
    icon2: asset("/icon-bicycle.png"),
    label: "A bicycle for coding",
    content: (
      <>
        <p>
          With predictable operations, ergonomic shortcuts, and tons of cases of
          “just doing the right thing,” CodePerfect does its job and gets out of
          the way.
        </p>
      </>
    ),
  },
];

const BAD_FEATURES = [
  "Electron",
  "JavaScript",
  "language servers",
  "browsers",
  "laggy runtimes",
  "bloated abstractions",
];

function Home() {
  return (
    <div className="mx-auto md:mt-24 w-full" style={{ maxWidth: "1920px" }}>
      <div className="max-w-full leading-relaxed p-8 pb-28">
        <div
          className="md:text-center text-5xl md:text-5xl mb-6 md:mb-1 tracking-wide text-white font-serif"
          style={{ lineHeight: "1.1" }}
        >
          A fast, powerful IDE for Go
        </div>

        <p className="md:text-center text-xl text-gray-500 leading-normal">
          Feature-rich and lightweight.
          <br />A power tool with a small resource footprint.
        </p>

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

      <div className="p-24 px-8 bg-neutral-900 pb-24 relative z-10 flex items-center justify-center">
        <div className="max-w-screen-2xl flex items-center gap-x-16">
          <div className="w-1/3">
            <div className="md:text-[250%] tracking-wide font-medium text-white leading-tight md:leading-tight font-title">
              {BAD_FEATURES.map((name) => (
                <div className="whitespace-nowrap">No {name}.</div>
              ))}
            </div>
            <div className="text-lg leading-normal mt-8 text-neutral-300">
              <p>
                We threw out the modern software stack and{" "}
                <A href={LINKS.handmadeManifesto}>handmade</A> the entire IDE
                stack in blazing fast C/C++. From the UI to the code indexer,
                everything runs as fast as a video game.
              </p>

              <p>
                A tool that assists your thinking instead of interfering with
                it. A barebones native app that does its job and gets out of
                your way.
              </p>
              <p></p>
            </div>
          </div>
          <div className="flex-1">
            <img
              className="max-w-full border border-neutral-500 shadow-lg rounded-lg overflow-hidden"
              alt="screenshot"
              src={asset("/download.png")}
            />
          </div>
        </div>
      </div>

      <div className="batteries-included z-10 px-6 md:px-12 py-16 md:py-32">
        <div className="batteries-included-child md:flex max-w-screen-xl md:gap-x-12 mx-auto">
          <div className="md:w-1/3 pb-6 md:pb-0 lg:pr-12">
            <h1 className="font-serif tracking-wide text-4xl text-black mb-6 mt-24">
              Batteries included
            </h1>
            <p className="text-lg leading-normal">
              Everything works out of the box with almost zero configuration.
              Enjoy the power of an IDE, bundled into an app that runs faster
              than Alacritty + tmux + vim.
            </p>
            <div className="mt-8">
              <A
                href={LINKS.docs}
                className="btn btn2 justify-center flex md:inline-flex text-center"
              >
                View Docs
                <Icon className="ml-2" icon={IconChevronRight} />
              </A>
            </div>
          </div>
          <div className="flex-1">
            <div className="grid grid-cols-2 md:grid-cols-3 gap-4 md:gap-6 mt-8 md:mt-0 md:p-0">
              {FEATURES.map((it) => (
                <div
                  className="bg-white hover:scale-[103%] shadow-sm rounded p-3 md:p-4 transition-all select-none"
                  key={it.label}
                >
                  <Icon stroke={1.25} size={26} icon={it.icon} />
                  <div className="leading-none font-semibold text-neutral-700 mt-1.5">
                    {it.label}
                  </div>
                  <div className="mt-1.5 font-sans text-[90%] text-neutral-400 leading-snug">
                    {it.desc}
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>

      <div className="bg-white py-16">
        <div className="grid grid-cols-1 md:grid-cols-3 max-w-screen-2xl mx-auto">
          {SELLING_POINTS.map((it, i) => (
            <div
              key={it.label}
              className="px-16 bg-white border-r border-neutral-200 last:border-0"
            >
              <div
                className={`w-16 h-16 rounded-xl bg-gradient-to-b shadow-lg opacity-70
              ${["from-zinc-600", "from-zinc-600", "from-zinc-600"][i]} 
              ${["to-zinc-500", "to-zinc-500", "to-stone-500"][i]} 
              flex items-center justify-center`}
              >
                <Icon
                  icon={it.icon}
                  stroke={1.0}
                  className="text-neutral-200"
                  size={42}
                />
              </div>
              <div className="text-lg font-bold text-neutral-800 mt-6 mb-2">
                {it.label}
              </div>
              <div className="leading-normal text-neutral-700">
                {it.content}
              </div>
            </div>
          ))}
        </div>
      </div>

      <div className="text-center py-24 bg-neutral-900">
        <div className="text-white tracking-wide text-4xl mb-2 font-title">
          Ready to get started?
        </div>
        <p className="mx-auto text-xl leading-relaxed mb-12 text-neutral-400">
          Try CodePerfect for free for 7 days with all features available.
        </p>
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
      recommended: true,
      name: "Pro",
      price: 10,
      features: [
        { label: "Commercial use ok" },
        { label: "All features available" },
        { label: "Company can pay" },
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
            You can download CodePerfect for a free, full-functionality 7 day
            trial before buying a license. No credit card is required.
          </p>
          <p>
            <A href="/download" className="btn btn1 btn-sm">
              Try CodePerfect for free
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
            have any other custom requests, please get in touch with us.
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
            Our support staff is happy to help. Please get in touch with us and
            we'll respond within 1 business day.
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

  return (
    <div className="mt-12 md:mt-24">
      <div className="text-center text-white font-serif text-3xl md:text-5xl mb-6 md:mb-24">
        Buy License
      </div>
      <div className="md:max-w-screen-xl mx-auto">
        <div className="mx-6 my-6 grid grid-cols-1 md:grid-cols-3 gap-6 md:gap-12 rounded">
          {plans.map(({ name, price, features, buttons, recommended }) => {
            const options = [
              [buttons.monthlyLink, "monthly", false],
              [buttons.yearlyLink, "yearly", true],
            ];

            return (
              <div
                className={cx(
                  twMerge(
                    cx(
                      "w-auto shadow-lg relative",
                      "rounded-lg",
                      recommended && "md:rounded-t-none md:rounded-b-lg",
                      recommended
                        ? "border border-primary"
                        : "border border-neutral-600"
                    )
                  )
                )}
              >
                {recommended && (
                  <div
                    className="md:absolute md:bottom-full md:left-0 md:right-0 rounded-t-lg bg-primary text-white text-base font-semibold leading-none p-3 text-center"
                    style={{ marginLeft: "-1px", marginRight: "-1px" }}
                  >
                    Most Popular*
                  </div>
                )}
                <div className="p-5">
                  <div className="text-neutral-300">
                    <div className="font-title text-lg font-medium text-neutral-300">
                      {name}
                    </div>
                    <div className="font-title font-semibold text-2xl text-white">
                      {price === "custom" ? "Custom pricing" : <>${price}/mo</>}
                    </div>
                  </div>
                  <div className="my-5">
                    {features.map((it) => (
                      <div
                        className={cx(
                          "flex items-center space-x-1.5 leading-6 mb-1 last:mb-0",
                          it.not ? "text-red-400" : "text-neutral-300"
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
                        className="btn btn2 block text-center py-3"
                        href={`mailto:${SUPPORT_EMAIL}`}
                      >
                        Contact support
                      </A>
                    ) : (
                      <div className="flex flex-col md:grid md:grid-cols-2 items-center gap-2">
                        {options.map(([link, unit, yearly]) => (
                          <span className="block w-full md:w-auto md:inline-block relative group">
                            {yearly && (
                              <span className="hidden group-hover:inline-block shadow absolute bottom-full mb-4 text-sm font-title font-semibold text-gray-500 right-1/2 whitespace-nowrap translate-x-1/2 w-auto rounded bg-neutral-100 text-neutral-500 py-2 px-3 leading-none">
                                <span
                                  class={twMerge(
                                    "w-0 h-0 border-transparent border-t-neutral-100 absolute top-full left-1/2 -translate-x-1/2"
                                  )}
                                  style={{ borderWidth: "6px" }}
                                />
                                2 months free!
                              </span>
                            )}
                            <A
                              className="btn btn1 py-3 block text-center"
                              href={link}
                            >
                              Buy {unit}
                            </A>
                          </span>
                        ))}
                      </div>
                    )}
                  </div>
                </div>
              </div>
            );
          })}
        </div>
        <div className="text-neutral-300 mt-4 mx-6 md:mx-0 md:text-center leading-tight text-base">
          * Many users expense the purchase with their employer.
        </div>
      </div>
      <div className="md:max-w-screen-xl mx-auto">
        <div className="mx-6 grid grid-cols-1 md:grid-cols-3 mt-12 md:mt-24 mb:4 md:my-24 md:border-t border-gray-100">
          {help.map((it) => (
            <div className="border-t md:border-t-0 md:border-r border-gray-100 md:first:border-l px-0 py-8 md:pt-8 md:pb-0 md:px-8">
              <div
                className="w-8 border-b-4 mb-2"
                style={{
                  borderColor: "rgb(11, 158, 245)",
                  filter: "saturate(0.35)",
                }}
              />
              <div className="font-title text-neutral-100 tracking-wide text-xl mb-2">
                {it.title}
              </div>
              <div className="text-gray-400 leading-normal">{it.content}</div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}

/*
function Question({ q, children }) {
  return (
    <>
      <p className="font-title font-bold">{q}</p>
      <p>{children}</p>
    </>
  );
}

function FAQ() {
  return (
    <WallOfText>
      <Title>Frequently Asked Questions</Title>
    </WallOfText>
  );
}
*/

function Download() {
  const links = [
    {
      platform: "windows-x64",
      icon: AiFillWindows,
      label: "Windows",
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
    <div className="my-12 md:my-28 px-8 md:px-0">
      <div className="font-normal tracking-wide md:px-4 md:text-center text-3xl md:text-5xl text-white font-serif">
        Download CodePerfect
      </div>
      <div className="text-center mt-4">
        <A
          href={`${LINKS.changelog}/${CURRENT_BUILD}`}
          className={cx(
            "flex items-start md:items:center md:justify-center flex-col md:flex-row",
            "gap-1 md:gap-4 no-underline opacity-90 hover:opacity-100 font-ui"
          )}
        >
          <span className="inline-block">
            <span className="font-semibold text-sm text-yellow-400 flex gap-x-1.5 items-center">
              <Icon size={18} icon={IconTag} />
              <span>Build {CURRENT_BUILD}</span>
            </span>
          </span>
          <span className="inline-block">
            <span className="font-semibold text-sm text-lime-300 gap-x-1.5 items-center flex">
              <Icon size={18} icon={IconClockHour4} />
              <span>Released {CURRENT_BUILD_RELEASE_DATE}</span>
            </span>
          </span>
        </A>
      </div>
      <div className="max-w-3xl mx-auto mt-12 md:px-4 md:text-center">
        <p className="flex flex-wrap flex-col md:flex-row justify-center">
          {links.map((it) => (
            <div className="p-3 border-l border-r border-t last:border-b md:border-0 md:border-t md:border-b md:border-l first:rounded-tl first:rounded-bl md:last:border-r last:rounded-tr last:rounded-br border-dashed border-gray-200">
              <A
                href={
                  it.disabledText
                    ? "#"
                    : `https://codeperfect95.s3.us-east-2.amazonaws.com/app/${it.platform}-${CURRENT_BUILD}.zip`
                }
                className={cx(
                  "btn btn1 flex md:inline-flex leading-none py-4 px-5",
                  it.disabledText && "disabled"
                )}
                title={it.disabledText}
                onClick={(e) => {
                  // disableButtonProps.onClick(e);
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
      <div className="px-4 mt-12 md:mt-20">
        <div
          // style={{ maxWidth: "calc(min(100%, 1024px))" }}
          className="max-w-screen-xl mx-auto my-8"
        >
          <Image src={asset("/get-started-screenshot.png")} />
        </div>
      </div>
      <div className="flex-grow"></div>
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

function Logo({ onClick }) {
  return (
    <A
      href="/"
      className="font-bold text-lg text-white no-underline whitespace-nowrap inline-flex flex-shrink-0 items-center"
      onClick={onClick}
    >
      <img
        alt="logo"
        className="w-auto h-8 inline-block mr-3 invert"
        src={asset("/logo.png")}
      />
      <span className="inline-block logo text-lg font-bold font-ui">
        CodePerfect 95
      </span>
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
    ["/buy", "Buy"],
    ["/download", "Download"],
  ];

  return (
    <div className="p-4 md:p-4 border-b border-gray-50 font-ui">
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
                className="absolute top-4 right-4 w-8 flex items-center justify-center h-8 rounded-full bg-neutral-700 text-white"
              >
                <Icon icon={IconX} />
              </button>
              <Logo onClick={() => setShowMenu(false)} />
              <div className="mt-2">
                {links.map(([url, label]) => (
                  <A
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
              className="text-neutral-300 hover:text-neutral-100 no-underline whitespace-nowrap hidden md:inline-block"
              // style={{ fontSize: "0.95em" }}
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
    <div className="px-4 pt-6 lg:pt-12 pb-8 md:pb-24 border-t border-gray-100 md:border-0">
      <div className="flex flex-col md:flex-row gap-y-4 md:gap-0 hmd:flex-row justify-between w-full md:max-w-screen-xl md:mx-auto items-start">
        <div className="text-gray-500">
          <span>&copy; {new Date().getFullYear()} CodePerfect 95</span>
        </div>
        <div className="flex flex-col md:flex-row md:items-start gap-y-3 md:gap-x-14 leading-none">
          <FootSection>
            <FootLink href="/buy">Buy License</FootLink>
            <FootLink href="/download">Download</FootLink>
            {/* <FootLink href="/faq">FAQs</FootLink> */}
          </FootSection>
          <FootSection>
            <FootLink href={LINKS.docs}>Docs</FootLink>
            <FootLink href={LINKS.changelog}>Changelog</FootLink>
            <FootLink href={LINKS.issueTracker}>Issue Tracker</FootLink>
          </FootSection>
          <FootSection>
            <FootLink href={`mailto:${SUPPORT_EMAIL}`}>Support</FootLink>
            <FootLink href="/terms">Terms &amp; Privacy</FootLink>
            <FootLink href={LINKS.mailingList}>Newsletter</FootLink>
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
            <Route path="buy" element={<BuyLicense />} />
            {/* <Route path="faq" element={<FAQ />} /> */}
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
