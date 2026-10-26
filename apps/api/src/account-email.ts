export type AccountEmailPurpose = "verification" | "password-reset";

const copy = {
  verification: {
    subject: "Verify your Neden email",
    heading: "Verify your email",
    introduction:
      "Confirm this email address to finish setting up your Neden account.",
    action: "Verify email",
    fallback: "Open the verification page",
  },
  "password-reset": {
    subject: "Reset your Neden password",
    heading: "Reset your password",
    introduction: "Choose a new password for your Neden account.",
    action: "Reset password",
    fallback: "Open the password reset page",
  },
} as const;

function escapeHtml(value: string) {
  return value.replace(/[&<>"']/g, (character) => {
    const entities: Record<string, string> = {
      "&": "&amp;",
      "<": "&lt;",
      ">": "&gt;",
      '"': "&quot;",
      "'": "&#39;",
    };
    return entities[character];
  });
}

export function accountEmail(purpose: AccountEmailPurpose, url: string) {
  const message = copy[purpose];
  const href = escapeHtml(url);
  const notice = "If you didn't request this email, you can safely ignore it.";
  return {
    subject: message.subject,
    text: `${message.heading}\n\n${message.introduction}\n\n${message.action}:\n${url}\n\n${notice}`,
    html: `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>${message.subject}</title>
</head>
<body style="margin:0;padding:0;background-color:#f4f6f8;color:#15191e;font-family:Arial,Helvetica,sans-serif;">
  <div style="display:none;max-height:0;overflow:hidden;opacity:0;mso-hide:all;">${message.introduction}</div>
  <table role="presentation" width="100%" cellspacing="0" cellpadding="0" border="0">
    <tr><td align="center" style="padding:32px 16px;">
      <table role="presentation" width="100%" cellspacing="0" cellpadding="0" border="0" style="max-width:520px;background-color:#ffffff;border:1px solid #dde3ea;border-radius:12px;">
        <tr><td style="padding:32px 24px;">
          <p style="margin:0 0 28px;color:#086bd9;font-size:20px;line-height:26px;font-weight:700;">Neden</p>
          <h1 style="margin:0 0 16px;font-size:28px;line-height:34px;font-weight:700;">${message.heading}</h1>
          <p style="margin:0 0 28px;font-size:16px;line-height:25px;color:#586474;">${message.introduction}</p>
          <table role="presentation" cellspacing="0" cellpadding="0" border="0">
            <tr><td bgcolor="#086bd9" style="border-radius:8px;">
              <a href="${href}" style="display:inline-block;padding:14px 24px;border:1px solid #086bd9;border-radius:8px;color:#ffffff;font-size:16px;line-height:20px;font-weight:700;text-align:center;text-decoration:none;">${message.action}</a>
            </td></tr>
          </table>
          <p style="margin:24px 0 0;font-size:14px;line-height:22px;color:#586474;">Button not working? <a href="${href}" style="color:#086bd9;text-decoration:underline;">${message.fallback}</a>.</p>
          <p style="margin:28px 0 0;padding-top:20px;border-top:1px solid #dde3ea;font-size:13px;line-height:21px;color:#586474;">${notice}</p>
        </td></tr>
      </table>
    </td></tr>
  </table>
</body>
</html>`,
  };
}
