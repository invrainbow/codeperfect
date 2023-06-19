import {
  IconArrowRight,
  IconBinaryTree,
  IconBrain,
  IconBulb,
  IconCalendar,
  IconCheck,
  IconChevronRight,
  IconCircleCheck,
  IconCircleMinus,
  IconClockHour4,
  IconCommand,
  IconDiamond,
  IconDownload,
  IconEdit,
  IconHexagons,
  IconMenu2,
  IconMessages,
  IconRobot,
  IconSearch,
  IconTag,
  IconTags,
  IconTelescope,
  IconTools,
  IconWand,
  IconX,
} from "@tabler/icons";

import { AiFillApple } from "@react-icons/all-files/ai/AiFillApple";
import { FaDiscord } from "@react-icons/all-files/fa/FaDiscord";
import { FaTwitter } from "@react-icons/all-files/fa/FaTwitter";
import { SiVim } from "@react-icons/all-files/si/SiVim";
// import { AiFillWindows } from "@react-icons/all-files/ai/AiFillWindows";
// import { FaLinux } from "@react-icons/all-files/fa/FaLinux";

import cx from "classnames";
import posthog from "posthog-js";
import React from "react";
import { Helmet } from "react-helmet";
import { twMerge } from "tailwind-merge";

import _ from "lodash";
import {
  BrowserRouter,
  Link,
  Navigate,
  Outlet,
  Route,
  Routes,
  useLocation,
} from "react-router-dom";
import "./index.css";

posthog.init("phc_kIt8VSMD8I2ScNhnjWDU2NmrK9kLIL3cHWpkgCX3Blw", {
  api_host: "https://app.posthog.com",
});

const SUPPORT_EMAIL = "support@codeperfect95.com";
const CURRENT_BUILD = process.env.REACT_APP_BUILD_VERSION;
const CURRENT_BUILD_RELEASE_DATE = "June 18, 2023";

const isDev = process.env.REACT_APP_CPENV === "development";
const isStaging = process.env.REACT_APP_CPENV === "staging";

const BASE_LINKS = {
  docs: "https://docs.codeperfect95.com",
  gettingStarted: "https://docs.codeperfect95.com/getting-started",
  changelog: "https://docs.codeperfect95.com/changelog",

  issueTracker: "https://github.com/codeperfect95/issue-tracker",
  twitter: "https://twitter.com/codeperfect95",
  discord: "https://discord.gg/WkFY44BY7a",
  substack: "https://codeperfect95.substack.com/",
  handmadeManifesto: "https://handmade.network/manifesto",
  nonPessimized: "https://www.youtube.com/watch?v=pgoetgxecw8",
  mikeActon: "https://www.youtube.com/watch?v=rX0ItVEVjHc",
  roadAhead: "https://codeperfect95.substack.com/p/the-road-ahead",
  oldSoftwareOpenedInstantly:
    "https://www.youtube.com/watch?v=GC-0tCy4P1U&t=2168s",

  payment: {
    individual: {
      license_only: "https://buy.stripe.com/cN27sF7vd6c8euk5ky",
      license_and_sub: {
        monthly: "https://buy.stripe.com/8wM9ANdTB0ROfyo3co",
        annual: "https://buy.stripe.com/7sI6oBaHpdEA0DufZb",
      },
      sub_only: {
        monthly: "https://buy.stripe.com/eVa14hbLteIEae428n",
        annual: "https://buy.stripe.com/4gwbIV8zh7gcae400g",
      },
    },
    business: {
      license_only: "https://buy.stripe.com/28oeV78zh7gc2LCbIZ",
      license_and_sub: {
        monthly: "https://buy.stripe.com/28ocMZeXF2ZWgCs5kE",
        annual: "https://buy.stripe.com/eVaeV716Pbwsdqg00l",
      },
      sub_only: {
        monthly: "https://buy.stripe.com/dR65kx6r9aso1HyeVc",
        annual: "https://buy.stripe.com/fZeaER5n5584gCs3cv",
      },
    },
  },
};

const DEV_LINKS = {
  docs: "http://localhost:3000",
  gettingStarted: "http://localhost:3000/getting-started",
  changelog: "http://localhost:3000/changelog",

  payment: {
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
  },
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
          A fast, lightweight Go IDE
        </div>
        <div className="md:text-center text-[120%] text-gray-600 leading-normal">
          CodePerfect is a power tool with a small resource footprint.
        </div>
        <div className="mt-8 md:mt-8 md:text-center flex flex-col md:flex-row gap-4 md:justify-center">
          <A
            href="/download"
            className="btn btn1 justify-center flex md:inline-flex text-center"
          >
            <Icon size={18} className="mr-1" icon={IconDownload} />
            Download for Mac
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
        <div className="max-w-screen-2xl flex flex-col lg:flex-row items-center gap-8 md:gap-12">
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
                We threw out the modern software stack and redesigned the IDE in
                blazing fast C/C++.
              </p>
              <p>
                Performance like a video game. Instant startup. 144 FPS. No
                latency between keystrokes. An indexer that gobbles through
                large codebases.
              </p>
              <p>
                With predictable operations, ergonomic shortcuts, and a
                streamlined workflow, CodePerfect does the right thing and gets
                out of your way.
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

      <div className="batteries-included z-10 px-6 md:px-12 py-12 md:py-20">
        <div className="max-w-screen-lg mx-auto flex flex-col md:flex-row gap-8 items-center">
          <div className="md:w-1/3">
            <h1 className="title text-3xl md:text-4xl">
              <div>Batteries included,</div>
              <div>zero configuration.</div>
            </h1>
            <div className="max-w-screen-sm mx-auto mt-4 mb-6 text-lg">
              <p>
                CodePerfect comes with a full IDE out of the box. Get the best
                of both worlds: the speed of Vim, the power of an IDE.
              </p>
            </div>
            <A
              href="/features"
              className="btn btn2 btn-lg btn-no-hover justify-center flex md:inline-flex text-center group"
            >
              View Features
              <Icon
                size={18}
                className="ml-1.5 relative top-[1px] group-hover:translate-x-1 transition"
                icon={IconChevronRight}
              />
            </A>
          </div>

          <div className="hidden md:block flex-1">
            <div className="flex flex-wrap justify-center gap-x-8 gap-y-3 text-lg font-medium rounded p-6 font-mono">
              {FEATURE_LIST.map((it) => (
                <div className="flex-shrink leading-none text-center text-neutral-300 hover:text-neutral-400 transition-colors">
                  {it.name}
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>

      <div className="md:text-center px-6 py-12 md:py-16 bg-white">
        <div className="title text-3xl md:text-4xl mb-3">
          Ready to get started?
        </div>
        <div className="mx-auto text-lg md:text-xl leading-relaxed mb-6 md:mb-12 text-neutral-600">
          Try CodePerfect for free for 7 days.
        </div>
        <A
          href="/download"
          className={twMerge(
            "btn btn1 btn-lg justify-center inline-flex text-center bg-black text-white px-6"
          )}
        >
          <Icon className="mr-2" icon={IconDownload} size={20} />
          Download for Mac
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
        Please check your email&mdash;we just sent your license key to you. If
        you don't see it, it might be in your spam folder. If you still don't
        see it, please <a href={`mailto:${SUPPORT_EMAIL}`}>contact support</a>.
      </p>
      <p>
        If you haven't done so yet, you can download CodePerfect{" "}
        <A href="/download">here</A>.
      </p>
      <p>
        We highly recommend subscribing to our{" "}
        <A href={LINKS.substack}>Substack</A> for essential product
        announcements and releases. We use it as our newsletter and blog rolled
        into one.
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
    <div className="bg-white shadow rounded-lg overflow-hidden p-5">
      <div className="leading-none mb-4 text-neutral-500 border-neutral-100 flex gap-2 items-center font-bold text-sm">
        {label}
      </div>
      {children}
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
      className={twMerge(
        cx(
          "font-inherit rounded text-left p-4 relative flex flex-col transition-colors bg-neutral-100 border-2 border-transparent",
          selected && "border-stone-500 bg-white"
        )
      )}
    >
      <div
        className={cx(
          "absolute top-2 right-2 w-6 h-6 flex items-center justify-center rounded-full bg-stone-600 text-white transition-opacity",
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
      label: "Pro",
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
        { label: <b>Everything in Pro</b> },
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
        "Version locked after 3 months",
      ],
    },
    {
      key: "license_and_sub",
      label: "License + Subscription",
      points: [
        "Perpetual license",
        "Automatic updates",
        "Version locked after subscription",
      ],
    },
    {
      key: "sub_only",
      label: "Subscription Only",
      points: [
        "Use software during susbcription",
        "Automatic updates",
        "Lose access after subscription",
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

  function getPaymentLink() {
    if (plan === "enterprise") {
      return null;
    }
    const ret = LINKS.payment[plan][product];
    if (product === "license_only") {
      return ret;
    }
    return ret[period];
  }

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

  const goodPoints = _.filter([
    isLicense && "Perpetual License",
    isSub && "Automatic updates",
    plan !== "individual" && "Company can pay",
  ]);

  const badPoints = _.filter([
    !isLicense && "Lose access after subscription",
    !isSub && "No updates after 3 months",
    plan === "individual" && "Company cannot pay",
  ]);

  return (
    <div className="mt-12 md:mt-24">
      <div className="title text-center text-3xl md:text-5xl mb-6 md:mb-12">
        Buy License
      </div>
      <div className="md:max-w-screen-xl mx-auto md:mb-16">
        <div className="flex flex-col md:flex-row gap-4 md:gap-8 md:items-start">
          <div className="flex-1 flex flex-col gap-6 px-4 lg:px-0">
            <BuyLicenseSection label="1. Select Plan">
              <div className="grid grid-cols-1 lg:grid-cols-3 gap-4">
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
                  <div className="grid grid-cols-1 lg:grid-cols-3 gap-4">
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
                    <div className="grid grid-cols-1 lg:grid-cols-2 gap-4">
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
          <div className="shadow-lg rounded-lg bg-neutral-800 p-8 mx-4 md:m-0 w-auto md:w-[350px]">
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
                <div className="pt-4 text-neutral-300">
                  Please get in touch with our support team to discuss your
                  options.
                </div>
              ) : (
                <>
                  <div className="mb-6 text-amber-300 font-bold">
                    {productInfo.label}
                  </div>
                  <div className="flex flex-col gap-1">
                    {goodPoints.map((it) => (
                      <BuySummaryPoint good text={it} />
                    ))}
                    {badPoints.map((it) => (
                      <BuySummaryPoint text={it} />
                    ))}
                  </div>
                </>
              )}
              <div className="mt-6 pt-6 border-t border-neutral-500 border-dashed">
                {plan === "enterprise" ? (
                  <div>
                    <A
                      className="group btn btn2 btn-no-hover btn-lg flex gap-2 items-center justify-center"
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
                        className="group btn btn2 btn-no-hover btn-lg flex gap-2 items-center justify-center"
                        style={{ paddingLeft: "40px", paddingRight: "40px" }}
                        href={getPaymentLink()}
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
      <div className="bg-zinc-50 border-y border-t-neutral-200 border-b-neutral-100 md:py-12 mt-8 md:mt-16">
        <div className="md:max-w-screen-xl mx-auto grid grid-cols-1 md:grid-cols-3 md:gap-6">
          {BUYING_QUESTIONS.map((it) => (
            <div className="border-b border-neutral-200 md:border-0 last:border-0 p-6 md:p-0">
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
          There isn't any one single thing. CodePerfect gets its speed largely
          by declining to copy all the extreme bloat that makes the modern tech
          stack slow. In particular, we:
        </p>
        <ol>
          <li style={{ marginBottom: "0.75em" }}>
            Use a low level language (C/C++) and render our own UI with OpenGL.
          </li>
          <li style={{ marginBottom: "0.75em" }}>
            Write straightforward,{" "}
            <A href={LINKS.nonPessimized}>non-pessimized</A> code that just
            executes the actual CPU instructions that do the thing it's supposed
            to.
            <br />
          </li>
          <li>Manage our own memory with amortized arena allocation.</li>
        </ol>
        <p>
          We especially try to limit use of third-party libraries and frameworks
          in order to own our entire stack, and maintain visibility into every
          line of code that goes into the final product. Much of the slowness of
          modern software comes not from this slow algorithm or that inefficient
          data structure, but from all the invisible bloat hidden inside the
          mainstream default software stack.
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
    q: "What's the long term goal?",
    a: (
      <>
        <p>
          We are trying to build the best power tool for the specific task of
          programming: editing, compiling, and debugging code. CodePerfect is
          tightly integrated and optimized around performing that task.
        </p>

        <p>
          We want to be <A href="https://sesuperhuman.com">Superhuman</A> for
          programming. Just like many professionals spend hours each day in
          their email, software engineers spend hours each day in their IDE, and
          all the little UX improvements add up.
        </p>
        <p>
          A big part of having a smooth UX is speed, or low latency, so that's a
          big initial focus.
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

const UNSORTED_FEATURE_LIST = [
  {
    name: "Autocomplete",
    body: (
      <>
        <p>
          <img src={asset("/feature-autocomplete.png")} alt="autocomplete" />
        </p>
        <p>
          Like any IDE, CodePerfect provides automatic completion to help you
          write code. Press <code>Ctrl+Space</code> to show all declared
          identifiers in the current scope, or press <code>.</code> after an
          identifier to see fields and methods.
        </p>
      </>
    ),
    icon: IconBrain,
  },
  {
    name: "Command Palette",
    body: (
      <>
        <p>
          <img
            src={asset("/feature-command-palette.png")}
            alt="command palette"
          />
        </p>
        <p>
          Every action in CodePerfect can be run as a command through the
          command palette. Just press <code>Cmd+K</code> anywhere to bring it
          up.
        </p>
        <p>
          The command palette supports fuzzy search so you can access commands
          quickly. It also displays the keyboard shortcut for future ease of
          use.
        </p>
      </>
    ),
    icon: IconCommand,
  },
  {
    name: "Global Fuzzy Finder",
    body: (
      <>
        <p>
          <img src={asset("/feature-goto-file.png")} alt="goto file" />
        </p>
        <p>
          Use fuzzy search to jump to any file or symbol. Press{" "}
          <code>Cmd+P</code> any time to open the file finder, and{" "}
          <code>Cmd+T</code> to open the symbol finder.
        </p>
        <p>
          We know navigation is a critical part of the developer workflow, so we
          specially made sure it was lag-free. From opening the finder, to
          typing each key, to selecting a file, there's no latency at any step.
        </p>
      </>
    ),
    icon: IconTelescope,
  },
  {
    name: "Format File",
    body: (
      <>
        <p>
          CodePerfect allows you to format your file at any time, as well as the
          option to automatically format on save.
        </p>
        <p>
          We use our in-memory index to organize your imports, removing unused
          imports and detecting which imports are needed to resolve undeclared
          symbols. This yields greater accuracy and speed.
        </p>
      </>
    ),
    icon: IconWand,
  },
  {
    name: "Postfix Completion",
    body: (
      <>
        <p>
          <img src={asset("/feature-postfix.png")} alt="postfix" />
        </p>
        <p>
          CodePerfect provides several macros in the autocomplete menu after you
          type <code>.</code> after an expression.
        </p>
        <p>
          These macros range from the powerful <code>.check!</code>, which
          assigns variables to the expression's return values, checks whether{" "}
          <code>err != nil</code>, and returns the zero-value of the current
          function's return type if so; to the straightforward{" "}
          <code>.ifnotnil</code>, which expands <code>x.ifnotnil!</code> to
        </p>
        <pre>
          if x != nil &#123;{"\n  "}
          {"// "}cursor here{"\n"}&#125;
        </pre>
      </>
    ),
    icon: IconRobot,
  },
  {
    name: "Jump to Definition",
    body: (
      <>
        <p>
          CodePerfect can jump to the definition of any declared symbol. Either
          move your cursor over it and press <code>Cmd+G</code> (or{" "}
          <code>gd</code> in Vim mode), or hold down <code>Cmd</code> and click
          it.
        </p>
      </>
    ),
    icon: IconBulb,
  },
  {
    name: "Manage Interfaces",
    body: (
      <>
        <p>
          <img
            src={asset("/feature-generate-implementation.png")}
            alt="generate implementation"
          />
        </p>
        <p>
          CodePerfect provides several facilities for navigating and working
          with interfaces:
        </p>
        <ul>
          <li>Given a type, list the interfaces it implements</li>
          <li>Given an interface, list the types that implement it</li>
          <li>Generate implementation of interface for type</li>
        </ul>
      </>
    ),
    icon: IconHexagons,
  },
  {
    name: "Build & Debug",
    body: (
      <>
        <p>
          CodePerfect is designed to make the edit-build-debug loop as seamless
          as possible.
        </p>
        <p>
          Trigger a build, jump to the first error, write a fix, jump to next
          error, fix, rebuild. Everything is done with ergonomic hotkeys. Error
          positions are preserved as you edit code. The entire experience is
          frictionless.
        </p>
        <p>
          When you're ready to debug, CodePerfect integrates with Delve to
          provide a seamless debugging experience.
        </p>
      </>
    ),
    icon: IconTools,
  },
  {
    name: "Global Live Search",
    body: (
      <>
        <p>
          Press <code>Cmd+Shift+F</code> to open project-wide search. Grepping a
          whole folder is fast enough on modern machines that we display results
          in realtime after each keystroke. It's like a faster, completely-local
          Algolia.
        </p>
        <p>
          When you see the result you want, use <code>Up</code> and{" "}
          <code>Down</code> to navigate results and <code>Enter</code> to
          select.
        </p>
        <p>
          We also support project-wide replace in the same window; press{" "}
          <code>Cmd+Shift+H</code>.
        </p>
      </>
    ),
    icon: IconSearch,
  },
  {
    name: "Rename Anything",
    body: (
      <>
        <p>
          CodePerfect can rename almost anything that's declared within your
          project. Just hover over an identifier and run the <code>Rename</code>{" "}
          command (or press <code>F12</code>). This works on any identifier, not
          just the actual declaration. It also works on struct field names.
        </p>
      </>
    ),
    icon: IconEdit,
  },
  {
    name: "Tree-Based Navigation",
    body: (
      <>
        <p>
          You can traverse your code by traversing its AST. Press{" "}
          <code>Ctrl+Alt+A</code> to enter tree-based navigation. Then,
        </p>
        <ul>
          <li>
            <code>Down</code> or <code>Right</code> to move to next sibling node
          </li>
          <li>
            <code>Up</code> or <code>Left</code> to move to previous sibling
            node
          </li>
          <li>
            <code>Shift+Down</code> or <code>Shift+Right</code> to move inward
            to child node
          </li>
          <li>
            <code>Shift+Up</code> or <code>Shift+Left</code> to move outward to
            parent node
          </li>
        </ul>
        <p>
          For Vim users, <code>h</code> <code>j</code> <code>k</code>{" "}
          <code>l</code> can be used instead of the arrow keys.
        </p>
      </>
    ),
    icon: IconBinaryTree,
  },
  {
    name: "Vim Keybindings",
    body: (
      <>
        <p>
          CodePerfect supports Vim keybindings out of the box. Go to{" "}
          <code>Tools</code> &gt; <code>Options</code> &gt;{" "}
          <code>Editor Settings</code> to enable them.
        </p>
        <p>
          Vim keybindings are a work in progress, but we currently support most
          of the commonly used commands, and the development team currently uses
          it for everyday text editing.
        </p>
      </>
    ),
    icon: SiVim,
  },
  {
    name: "Video Game Performance",
    body: (
      <>
        <p>
          CodePerfect was designed from the ground up with performance in mind.
          Instant startup, no latency, 144 frames a second, runs as smooth as a
          video game.
        </p>
      </>
    ),
  },
  {
    name: "Generate Function",
    body: (
      <>
        <p>
          Sometimes you're coding and you wish that a function or method
          existed, so you make a note to implement it later.
        </p>
        <p>
          With CodePerfect, you can write a function call, and CodePerfect can
          generate the function signature based on the types of the parameters.
          For instance, if you have
        </p>
        <pre>
          x := 0{"\n"}y := false{"\n"}calculate(x, y)
        </pre>
        <p>
          you can hover over <code>calculate</code> and run{" "}
          <code>Generate Function From Call</code>. It will generate
        </p>
        <pre>func calculate(v0 int, v1 bool)</pre>
      </>
    ),
    icon: IconDiamond,
  },
  {
    name: "Find References",
    body: (
      <>
        <p>
          <img
            src={asset("/feature-find-references.png")}
            alt="find references"
          />
        </p>
        <p>
          Find References works as it does in other IDEs. Hover over the name
          and press <code>Cmd+Alt+R</code> or run <code>Find References</code>{" "}
          in the command palette.
        </p>
      </>
    ),
    icon: IconTags,
  },
  {
    name: "Manage Struct Tags",
    body: (
      <>
        <p>
          CodePerfect comes with commands to let you generate, add and remove
          tags from structs. Just search the command palette for{" "}
          <code>struct</code>.
        </p>
      </>
    ),
    icon: IconTags,
  },
];

const FEATURE_LIST = _.sortBy(UNSORTED_FEATURE_LIST, "name");

function Features() {
  const onScroll = (name) => {
    const elem = document.querySelector(`[data-feature-name="${name}"]`);
    if (elem) {
      elem.scrollIntoView({
        behavior: "smooth",
      });
    }
  };

  return (
    <div className="max-w-screen-lg flex mx-auto mt-8 border-b border-neutral-100 md:border-0 md:my-16 gap-12 features">
      <div className="w-[175px] hidden md:block">
        <div className="sticky top-8">
          {FEATURE_LIST.map((it) => (
            <div className="mb-3 last:mb-0 leading-none">
              <button
                onClick={() => onScroll(it.name)}
                className="p-0 leading-none font-medium text-neutral-600 hover:text-neutral-900 text-left"
                key={it.name}
              >
                {it.name}
              </button>
            </div>
          ))}
        </div>
      </div>
      <div className="flex-1">
        <div className="px-5 md:px-0">
          <div className="title text-3xl md:text-5xl mb-4">Features</div>
          <div className="mb-8">
            This is a brief overview of the features inside CodePerfect. For a
            more complete and in-depth list, see the full{" "}
            <A href={LINKS.docs}>docs</A>.
          </div>
        </div>
        <div className="">
          {FEATURE_LIST.map((it) => (
            <div
              data-feature-name={it.name}
              key={it.name}
              className="bg-white md:rounded-lg md:shadow-lg md:mt-8 p-6 border-b last:border-0 border-neutral-100 md:border-0 md:p-8 md:first:mt-0"
            >
              <div className="mb-4 font-bold text-lg leading-none">
                {it.name}
              </div>
              {it.body}
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}

function FAQ() {
  // const [states, setStates] = React.useState({});

  return (
    <div className="bg-white md:bg-transparent py-12 px-6 md:px-4 md:py-24 md:max-w-screen-sm mx-auto">
      <div className="md:px-4 md:text-center text-3xl md:text-5xl title mb-8">
        FAQ
      </div>
      <div className="flex flex-col md:gap-4">
        {faqs.map((it, i) => (
          <div
            className="prose md:bg-white border-b border-neutral-100 last:border-0 md:border-0 md:rounded-lg md:shadow-sm group py-6 first:pt-0 last:pb-0 md:p-6 md:first:p-6 md:last:p-6"
            key={it.q}
          >
            <div className="mb-5 font-bold">{it.q}</div>
            {it.a}
          </div>
        ))}
      </div>
    </div>
  );
}

function Download() {
  const links = [
    /*
    {
      platform: "windows-x64",
      icon: AiFillWindows,
      label: "Windows",
      disabledText: "Temporarily unavailable.",
    },
    */
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
    /*
    {
      platform: "linux-x64",
      icon: FaLinux,
      label: "Linux",
      disabledText: "Coming soon!",
    },
    */
  ];

  return (
    <div className="my-12 md:my-28 px-6 md:px-0">
      <div className="md:px-4 md:text-center text-3xl md:text-5xl title leading-none">
        CodePerfect for Mac
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
          <div className="mt-4">
            <span className="rounded-full text-sm font-semibold bg-neutral-200 text-neutral-500 py-1.5 px-4 inline-flex items-center gap-1.5">
              <Icon icon={IconCalendar} size={20} />
              <span className="relative" style={{ top: 1 }}>
                Windows &amp; Linux coming soon!
              </span>
            </span>
          </div>
        </p>
        <div className="flex items-center justify-center">
          <div className="mt-8">
            CodePerfect is free to evaluate for 7 days. Then you'll need a{" "}
            <A href="/buy">license</A>.
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
    ["/features", "Features"],
    ["/faq", "FAQ"],
    ["/buy", "Buy"],
    ["/download", "Download"],
    [LINKS.discord, <Icon size={24} icon={FaDiscord} />],
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
              <div className="mt-2 flex flex-col md:flex-row md:items-center">
                {links.map(([url, label]) => (
                  <A
                    key={url}
                    className="flex text-neutral-100 no-underline whitespace-nowrap md:hidden leading-none py-2 md:items-center"
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
        <div className="hidden md:flex items-center gap-x-8">
          {links.map(([url, label]) => (
            <A
              key={url}
              className="text-[95%] text-neutral-700 no-underline whitespace-nowrap hidden md:inline-flex"
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
            <FootLink href="/features">Features</FootLink>
            <FootLink href="/buy">Buy License</FootLink>
            <FootLink href="/download">Download for Mac</FootLink>
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
            <Route path="features" element={<Features />} />
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
