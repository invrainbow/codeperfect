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
  IconMessages,
  IconCircleCheck,
  IconCircleMinus,
  IconArrowRight,
  IconCalendarTime,
  IconShoppingCart,
  IconUsers,
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
const CURRENT_BUILD_RELEASE_DATE = "March 20, 2023";

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

const BUYING_QUESTIONS = [
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
          If you need multiple licenses, volume pricing, team management, or
          have any other custom requests, please reach out to us.
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

function BuyLicenseSection({ label, children }) {
  return (
    <div className="bg-white shadow rounded-lg overflow-hidden">
      <div className="leading-none p-4 border-b border-neutral-200 flex gap-2 items-center font-bold">
        {label}
      </div>
      <div className="p-4">{children}</div>
    </div>
  );
}

function BuyLineItem({ label, value, total }) {
  return (
    <div
      className={cx(
        "flex justify-between py-2 first:pt-0 last:pb-0",
        !total && "border-b border-neutral-600 last:border-0"
      )}
    >
      <div className={cx(total && "font-bold")}>{label}</div>
      <div className={cx(total && "font-bold text-xl")}>{value}</div>
    </div>
  );
}

function BuySummaryPoint({ good, text }) {
  return (
    <div
      className={cx(
        "flex items-center gap-2",
        good ? "text-neutral-100" : "text-neutral-400"
      )}
    >
      <span className="leading-none">
        <Icon size={22} icon={good ? IconCircleCheck : IconCircleMinus} />
      </span>
      <span>{text}</span>
    </div>
  );
}

function BuySelectable({ selected, label, children, onClick }) {
  return (
    <button
      onClick={onClick}
      className={cx(
        "font-inherit rounded-lg text-left p-5 relative flex flex-col bg-white transition-colors shadow",
        {
          "border border-neutral-300 bg-neutral-50 ": !selected,
          "border border-neutral-500": selected,
        }
      )}
    >
      <div
        className={cx(
          "absolute top-2 right-2 w-6 h-6 flex items-center justify-center rounded-full bg-neutral-500 text-white transition-opacity",
          {
            "opacity-100": selected,
            "opacity-0": !selected,
          }
        )}
      >
        <Icon size={18} icon={IconCheck} />
      </div>
      <div className="font-bold transition-colors text-black">{label}</div>
      {children && <div className={cx("mt-1")}>{children}</div>}
    </button>
  );
}

function BuyLicense() {
  const PLAN_INFO = [
    {
      key: "individual",
      label: "Personal",
      features: [{ label: "Commercial use ok" }],
    },
    {
      key: "business",
      label: "Business",
      features: [
        { label: <b>Everything in Personal</b> },
        { label: "Company can pay" },
        { label: "Purchase can be expensed" },
        { label: "Priority support" },
      ],
    },
    {
      key: "enterprise",
      label: "Enterprise",
      custom: true,
      features: [
        { label: <b>Everything in Business</b> },
        { label: "Volume pricing" },
        { label: "Team licensing" },
        { label: "Other custom requests" },
      ],
    },
  ];

  const PRODUCT_INFO = [
    {
      key: "license_only",
      label: "License Only",
      points: [
        "Perpetual license",
        "3 months of updates included",
        "Locked to version at end of 3 months",
      ],
    },
    {
      key: "license_and_sub",
      label: "License + Subscription",
      points: [
        "Perpetual license",
        "Automatic updates",
        "Locked to version at end of subscription",
      ],
    },
    {
      key: "sub_only",
      label: "Subscription Only",
      points: [
        '"Rent" the software',
        "Automatic updates",
        "Lose access when subscription ends",
      ],
    },
  ];

  const PERIOD_INFO = [
    {
      key: "monthly",
      label: "Monthly Billing",
    },
    {
      key: "annual",
      label: "Annual Billing",
      subtitle: "2 months off",
    },
  ];

  const [plan, setPlan] = React.useState("individual");
  const [product, setProduct] = React.useState("license_and_sub");
  const [period, setPeriod] = React.useState("monthly");

  const planInfo = PLAN_INFO.filter((it) => it.key === plan)[0];
  const productInfo = PRODUCT_INFO.filter((it) => it.key === product)[0];

  const isLicense = product !== "sub_only";
  const isSub = product !== "license_only";

  const PAYMENT_LINKS = {
    individual: {
      license_only: "https://buy.stripe.com/test_cN2293fhg9KffiU4gt",
      license_and_sub: {
        monthly: "https://buy.stripe.com/test_dR64hbglke0vgmYeV5",
        annual: "https://buy.stripe.com/test_6oEaFzfhg1dJc6IeVe",
      },
      sub_only: {
        monthly: "https://buy.stripe.com/test_fZe6pjd987C7fiUdR4",
        annual: "https://buy.stripe.com/test_00g2939WW4pV3Ac6oF",
      },
    },
    business: {
      license_only: "https://buy.stripe.com/test_bIYcNH3yy2hN0o07sI",
      license_and_sub: {
        monthly: "https://buy.stripe.com/test_aEU2933yy2hNb2E00c",
        annual: "https://buy.stripe.com/test_aEU293fhg9Kfc6I6oH",
      },
      sub_only: {
        monthly: "https://buy.stripe.com/test_7sI6pj7OOcWr8UwbIX",
        annual: "https://buy.stripe.com/test_4gwcNH7OO3lR8Uw14m",
      },
    },
  };

  function getPaymentLink() {
    if (plan === "enterprise") {
      return null;
    }
    const ret = PAYMENT_LINKS[plan][product];
    if (product === "license_only") {
      return ret;
    }
    return ret[period];
  }

  const paymentLink = getPaymentLink();
  console.log(paymentLink);

  function calcLicensePrice() {
    if (!isLicense) return 0;
    return plan === "individual" ? 39.99 : 119.99;
  }

  function calcSubPrice() {
    if (!isSub) return 0;
    if (period === "monthly") {
      return plan === "individual" ? 4.99 : 14.99;
    }
    return plan === "individual" ? 49.99 : 149.99;
  }

  function formatMoney(amt) {
    const formatter = new Intl.NumberFormat("en-US", {
      style: "currency",
      currency: "USD",
    });
    return formatter.format(amt);
  }

  const licensePrice = calcLicensePrice();
  const subPrice = calcSubPrice();

  return (
    <div className="mt-12 md:mt-24">
      <div className="title text-center text-3xl md:text-5xl mb-6 md:mb-12">
        Buy License
      </div>
      <div className="md:max-w-screen-xl mx-auto md:mb-16">
        <div className="flex gap-8 items-start">
          <div className="flex-1 flex flex-col gap-4">
            <BuyLicenseSection label="1. Select Plan">
              <div className="grid grid-cols-3 gap-3">
                {PLAN_INFO.map(({ label, features, key }) => (
                  <BuySelectable
                    key={key}
                    onClick={() => setPlan(key)}
                    label={label}
                    selected={plan === key}
                  >
                    <div>
                      {features.map((it) => (
                        <div
                          className={cx(
                            "flex items-center space-x-1.5 leading-6 mb-0.5 last:mb-0 text-[95%]",
                            {
                              "text-red-400": it.not,
                              "text-neutral-600": !it.not,
                            }
                          )}
                        >
                          <Icon size={18} icon={it.not ? IconX : IconCheck} />
                          <span>{it.label}</span>
                        </div>
                      ))}
                    </div>
                  </BuySelectable>
                ))}
              </div>
            </BuyLicenseSection>
            {plan !== "enterprise" && (
              <>
                <BuyLicenseSection label="2. Select Product">
                  <div className="grid grid-cols-3 gap-3">
                    {PRODUCT_INFO.map(({ key, label, points }) => (
                      <BuySelectable
                        key={key}
                        onClick={() => setProduct(key)}
                        selected={product === key}
                        label={label}
                      >
                        <ul style={{ marginTop: 0 }} className="pl-2 pt-1">
                          {points.map((it) => (
                            <li className="leading-tight mb-1 last:mb-0 text-[95%]">
                              {it}
                            </li>
                          ))}
                        </ul>
                      </BuySelectable>
                    ))}
                  </div>
                </BuyLicenseSection>
                {product !== "license_only" && (
                  <BuyLicenseSection label="3. Select Billing Period">
                    <div className="grid grid-cols-2 gap-3">
                      {PERIOD_INFO.map(({ key, label, subtitle }) => (
                        <BuySelectable
                          key={key}
                          onClick={() => setPeriod(key)}
                          selected={period === key}
                          label={
                            <div>
                              {label}
                              {subtitle && (
                                <span className="font-light text-neutral-400 text-sm ml-2">
                                  ({subtitle})
                                </span>
                              )}
                            </div>
                          }
                        />
                      ))}
                    </div>
                  </BuyLicenseSection>
                )}
              </>
            )}
          </div>
          <div className="shadow-lg rounded-lg bg-neutral-800 p-8 w-[350px]">
            <div className="max-w-screen-md mx-auto">
              <div className="flex flex-col">
                <div className="leading-none">
                  <span className="text-white font-bold text-xl leading-none">
                    CodePerfect
                  </span>
                  <span className="text-neutral-300 text-lg ml-1.5">
                    {planInfo.label}
                  </span>
                </div>
                {plan === "enterprise" ? (
                  <div className="pt-4 text-neutral-300 leading-tight">
                    Please get in touch with our support team to discuss your
                    options.
                  </div>
                ) : (
                  <>
                    <div className="mb-6 text-emerald-400 font-bold">
                      {productInfo.label}
                    </div>
                    <div className="flex flex-col gap-1">
                      {isLicense && (
                        <BuySummaryPoint good text="Perpetual license" />
                      )}
                      {isSub ? (
                        <>
                          <BuySummaryPoint good text="Automatic updates" />
                          <BuySummaryPoint
                            good
                            text={
                              period === "monthly"
                                ? "Monthly billing"
                                : "Annual billing"
                            }
                          />
                        </>
                      ) : (
                        <BuySummaryPoint text="No updates after 3 months" />
                      )}
                      {!isLicense && (
                        <BuySummaryPoint text="Lose access after subscription" />
                      )}
                      {plan === "individual" && (
                        <BuySummaryPoint text="Company cannot pay" />
                      )}
                    </div>
                  </>
                )}
                <div className="mt-6 pt-6 border-t border-neutral-500 border-dashed">
                  {plan === "enterprise" ? (
                    <div>
                      <A
                        className="group btn btn2 btn-no-hover btn-lg inline-flex gap-2 items-center justify-center"
                        href={`mailto:${SUPPORT_EMAIL}`}
                      >
                        <Icon size={16} icon={IconMessages} />
                        <span>Contact support</span>
                      </A>
                    </div>
                  ) : (
                    <>
                      <div className="text-white border-neutral-400 border rounded-lg p-4">
                        {isLicense && (
                          <BuyLineItem
                            label="License"
                            value={formatMoney(licensePrice)}
                          />
                        )}
                        {isSub && (
                          <BuyLineItem
                            label={
                              period === "monthly"
                                ? "Monthly subscription"
                                : "Annual subscription"
                            }
                            value={`${formatMoney(subPrice)}/${
                              period === "monthly" ? "mo" : "yr"
                            }`}
                          />
                        )}
                        <BuyLineItem
                          label="Due today"
                          total
                          value={formatMoney(licensePrice + subPrice)}
                        />
                      </div>
                      <div className="mt-6">
                        <A
                          className="group btn btn2 btn-no-hover btn-lg inline-flex gap-2 items-center justify-center"
                          style={{ paddingLeft: "40px", paddingRight: "40px" }}
                          href={paymentLink}
                        >
                          Checkout
                          <Icon
                            className="group-hover:translate-x-1 transform transition-transform"
                            icon={IconArrowRight}
                          />
                        </A>
                      </div>
                    </>
                  )}
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
      <div className="bg-zinc-50 border-y border-t-neutral-200 border-b-neutral-100 py-12 mt-16">
        <div className="md:max-w-screen-xl mx-auto grid grid-cols-1 md:grid-cols-3 gap-12 md:gap-6">
          {BUYING_QUESTIONS.map((it) => (
            <div className="">
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
    q: "What makes it fast?",
    a: (
      <>
        <p>
          There isn't any one thing. CodePerfect manages to be fast compared to
          modern software by declining to copy what makes modern software slow.
        </p>

        <p>
          The modern tech stack has extreme bloat everywhere. We eschewed all
          that in favor of a low level language (C/C++) and managing our own
          memory with amortized arena allocation. We write straightforward,{" "}
          <A href={LINKS.nonPessimized}>non-pessimized</A> code that just
          executes the actual CPU instructions that do the thing it's supposed
          to.
        </p>

        <p>
          We're not writing crazy inline assembly or SIMD intrinsics or
          discovering new algorithms or whatever. We do some optimization, like
          using file mappings and multithreading stuff where it makes sense, but
          mostly we are just writing straightforward code that performs the
          actual task of executing an IDE. Modern computers are just fast.
        </p>
      </>
    ),
  },
  {
    q: "How does it compare with Jetbrains or VSCode?",
    a: (
      <>
        <p>
          Jetbrains has more features, VSCode is free and customizable, and
          CodePerfect is fast.
        </p>

        <p>
          Right now we're targeting people who want an IDE as fast as Vim, but
          comes with code intelligence and other IDE features to program
          productively. Our users spend a lot of time in their editor, and get
          significant value and joy from a seamless, latency-free workflow.
        </p>
        <p>
          In exchange, CodePerfect has fewer features, is not free, and provides
          limited customization.
        </p>
      </>
    ),
  },
  {
    q: "What's the long term goal?",
    a: (
      <>
        <p>We are trying to build the best power tool for programming.</p>
        <p>
          New programming tools today tend to have ambitious goals like making
          programming more collaborative, or involve less code, or more
          integrated with third-party tools. Ours is more boring: to build the
          best tool for editing, compiling, and debugging code. CodePerfect is
          tightly integrated and optimized around that workflow.
        </p>

        <p>
          We want to be <A href="https://sesuperhuman.com">Superhuman</A> for
          programming. Business people are in their email all day, and small
          improvements add up. Same with an IDE.
        </p>
        <p>
          A big part of this is building a smooth experience. Up to a point,
          that means speed, or low latency, so that's a big initial focus.
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
          on hardware much slower than a phone today.
        </p>
      </>
    ),
  },
];

function FAQ() {
  const [states, setStates] = React.useState({});

  return (
    <div className="bg-white md:bg-transparent py-12 px-6 md:px-4 md:py-24 md:max-w-screen-sm mx-auto">
      <div className="md:px-4 md:text-center text-3xl md:text-5xl title mb-8">
        FAQ
      </div>
      <div className="flex flex-col gap-4 md:gap-4">
        {faqs.map((it, i) => (
          <div
            className="prose md:bg-white border-b border-neutral-200 last:border-0 md:border-0 md:rounded-lg md:shadow-sm"
            key={it.q}
          >
            <button
              onClick={() => setStates({ ...states, [i]: !states[i] })}
              className="text-left pb-4 md:p-5 font-bold w-full flex justify-between items-center"
            >
              <div>{it.q}</div>
              <div className="flex items-center justify-center">
                <Icon
                  size={20}
                  icon={IconChevronDown}
                  className={cx(
                    "transition/-transform origin-center",
                    states[i] && "rotate-180"
                  )}
                />
              </div>
            </button>
            {states[i] && <div className="pb-4 md:p-5 md:pt-0">{it.a}</div>}
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
